#include<bplus_tree.h>

#include<bplus_tree_split_insert_util.h>
#include<bplus_tree_merge_util.h>
#include<bplus_tree_find_for_update_util.h>
#include<bplus_tree_page_header.h>
#include<persistent_page_functions.h>
#include<sorted_packed_page_util.h>
#include<storage_capacity_page_util.h>

#include<stdlib.h>

int inspected_update_in_bplus_tree(uint64_t root_page_id, void* new_record, const update_inspector* ui_p, const bplus_tree_tuple_defs* bpttd_p, const data_access_methods* dam_p, const page_modification_methods* pmm_p, const void* transaction_id, int* abort_error)
{
	if(!check_if_record_can_be_inserted_into_bplus_tree(bpttd_p, new_record))
		return 0;

	// create a stack of capacity = levels
	locked_pages_stack* locked_pages_stack_p = &((locked_pages_stack){});
	uint32_t root_page_level;

	{
		// get lock on the root page of the bplus_tree
		persistent_page root_page = acquire_persistent_page_with_lock(dam_p, transaction_id, root_page_id, WRITE_LOCK, abort_error);
		if(*abort_error)
			return 0;

		// pre cache level of the root_page
		root_page_level = get_level_of_bplus_tree_page(&root_page, bpttd_p);

		// create a stack of capacity = levels
		if(!initialize_locked_pages_stack(locked_pages_stack_p, root_page_level + 1))
			exit(-1);

		// push the root page onto the stack
		push_to_locked_pages_stack(locked_pages_stack_p, &INIT_LOCKED_PAGE_INFO(root_page));
	}

	uint32_t release_for_split = 0;
	uint32_t release_for_merge = 0;
	walk_down_locking_parent_pages_for_update_using_record(root_page_id, locked_pages_stack_p, new_record, &release_for_split, &release_for_merge, bpttd_p, dam_p, transaction_id, abort_error);
	if(*abort_error)
		return 0;

	// concerned_leaf will always be at the top of this stack
	persistent_page* concerned_leaf = &(get_top_of_locked_pages_stack(locked_pages_stack_p)->ppage);

	// find index of last record that compares equal to the new_record
	uint32_t found_index = find_last_in_sorted_packed_page(
											concerned_leaf, bpttd_p->page_size,
											bpttd_p->record_def, bpttd_p->key_element_ids, bpttd_p->key_compare_direction, bpttd_p->key_element_count,
											new_record, bpttd_p->record_def, bpttd_p->key_element_ids
										);

	// get the reference to the old_record
	void* old_record = NULL;
	uint32_t old_record_size = 0;
	if(NO_TUPLE_FOUND != found_index)
	{
		old_record = (void*) get_nth_tuple_on_persistent_page(concerned_leaf, bpttd_p->page_size, &(bpttd_p->record_def->size_def), found_index);
		old_record_size = get_tuple_size(bpttd_p->record_def, old_record);
	}

	// result to return
	int result = 0;

	int ui_res = ui_p->update_inspect(ui_p->context, bpttd_p->record_def, old_record, &new_record, transaction_id, abort_error);
	if((*abort_error) || ui_res == 0)
	{
		result = 0;
		goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;
	}

	// if old_record did not exist and the new_record is set to NULL (i.e. a request for deletion, then do nothing)
	if(old_record == NULL && new_record == NULL)
	{
		result = 1;
		goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;
	}
	else if(old_record == NULL && new_record != NULL) // insert case
	{
		// check again if the new_record is small enough to be inserted on the page
		if(!check_if_record_can_be_inserted_into_bplus_tree(bpttd_p, new_record))
		{
			result = 0;
			goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;
		}

		// we can release lock on release_for_split number of parent pages
		while(release_for_split > 0)
		{
			locked_page_info* bottom = get_bottom_of_locked_pages_stack(locked_pages_stack_p);
			release_lock_on_persistent_page(dam_p, transaction_id, &(bottom->ppage), NONE_OPTION, abort_error);
			pop_bottom_from_locked_pages_stack(locked_pages_stack_p);

			if(*abort_error)
				goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;

			release_for_split--;
		}

		result = split_insert_and_unlock_pages_up(root_page_id, locked_pages_stack_p, new_record, bpttd_p, dam_p, pmm_p, transaction_id, abort_error);

		// deinitialize stack, all page locks will be released, after return of the above function
		deinitialize_locked_pages_stack(locked_pages_stack_p);

		// on abort, result will be 0
		return result;
	}
	else if(old_record != NULL && new_record == NULL) // delete case
	{
		// we can release lock on release_for_merge number of parent pages
		while(release_for_merge > 0)
		{
			locked_page_info* bottom = get_bottom_of_locked_pages_stack(locked_pages_stack_p);
			release_lock_on_persistent_page(dam_p, transaction_id, &(bottom->ppage), NONE_OPTION, abort_error);
			pop_bottom_from_locked_pages_stack(locked_pages_stack_p);

			if(*abort_error)
				goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;

			release_for_merge--;
		}

		// perform a delete operation on the found index in this page, this has to succeed
		delete_in_sorted_packed_page(
							concerned_leaf, bpttd_p->page_size,
							bpttd_p->record_def,
							found_index,
							pmm_p,
							transaction_id,
							abort_error
						);
		if(*abort_error)
			goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;

		result = merge_and_unlock_pages_up(root_page_id, locked_pages_stack_p, bpttd_p, dam_p, pmm_p, transaction_id, abort_error);

		// deinitialize stack, all page locks will be released, after return of the above function
		deinitialize_locked_pages_stack(locked_pages_stack_p);

		// on abort, result will be 0
		return result;
	}
	else // update
	{
		// fail if the keys of old_record and new_record, do not match then quit
		if(0 != compare_tuples(old_record, bpttd_p->record_def, bpttd_p->key_element_ids, new_record, bpttd_p->record_def, bpttd_p->key_element_ids, bpttd_p->key_compare_direction, bpttd_p->key_element_count))
		{
			result = 0;
			goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;
		}

		// check again if the new_record is small enough to be inserted on the page
		if(!check_if_record_can_be_inserted_into_bplus_tree(bpttd_p, new_record))
		{
			result = 0;
			goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;
		}

		// compute the size of the new record
		uint32_t new_record_size = get_tuple_size(bpttd_p->record_def, new_record);

		int updated = update_at_in_sorted_packed_page(
									concerned_leaf, bpttd_p->page_size, 
									bpttd_p->record_def, bpttd_p->key_element_ids, bpttd_p->key_compare_direction, bpttd_p->key_element_count,
									new_record, 
									found_index,
									pmm_p,
									transaction_id,
									abort_error
								);
		if(*abort_error)
			goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;

		if(new_record_size != old_record_size) // inplace update
		{
			if(new_record_size > old_record_size) // may require split
			{
				if(!updated) // then this leaf must split
				{
					// first perform a delete and then a split insert
					// this function may not fail, because our found index is valid
					delete_in_sorted_packed_page(
							concerned_leaf, bpttd_p->page_size,
							bpttd_p->record_def,
							found_index,
							pmm_p,
							transaction_id,
							abort_error
						);
					if(*abort_error)
						goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;

					// we can release lock on release_for_split number of parent pages
					while(release_for_split > 0)
					{
						locked_page_info* bottom = get_bottom_of_locked_pages_stack(locked_pages_stack_p);
						release_lock_on_persistent_page(dam_p, transaction_id, &(bottom->ppage), NONE_OPTION, abort_error);
						pop_bottom_from_locked_pages_stack(locked_pages_stack_p);

						if(*abort_error)
							goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;

						release_for_split--;
					}

					// once the delete is done, we can split insert the new_record
					result = split_insert_and_unlock_pages_up(root_page_id, locked_pages_stack_p, new_record, bpttd_p, dam_p, pmm_p, transaction_id, abort_error);

					// deinitialize stack, all page locks will be released, after return of the above function
					deinitialize_locked_pages_stack(locked_pages_stack_p);

					// on abort, result will be 0
					return result;
				}
				else
				{
					// the record was big, yet we could fit it on the page, nothing to be done really
					result = 1;
					goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;
				}
			}
			else // new_record_size <= old_record_size -> may require merge -> but it is assumed to be true that updated = 1
			{
				// we can release lock on release_for_merge number of parent pages
				while(release_for_merge > 0)
				{
					locked_page_info* bottom = get_bottom_of_locked_pages_stack(locked_pages_stack_p);
					release_lock_on_persistent_page(dam_p, transaction_id, &(bottom->ppage), NONE_OPTION, abort_error);
					pop_bottom_from_locked_pages_stack(locked_pages_stack_p);

					if(*abort_error)
						goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;

					release_for_merge--;
				}

				result = merge_and_unlock_pages_up(root_page_id, locked_pages_stack_p, bpttd_p, dam_p, pmm_p, transaction_id, abort_error);

				// deinitialize stack, all page locks will be released, after return of the above function
				deinitialize_locked_pages_stack(locked_pages_stack_p);

				// on abort, result will be 0
				return result;
			}
		}
		else // we do not need to do any thing further, if the records are of same size
		{
			// set return value to 1, and RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT will take care of the rest
			result = 1;
			goto RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT;
		}
	}


	RELEASE_LOCKS_DEINITIALIZE_STACK_AND_EXIT:;
	// release all locks of the locked_pages_stack and exit
	// do not worry about abort_error here or in the loop, as we are only releasing locks
	while(get_element_count_locked_pages_stack(locked_pages_stack_p) > 0)
	{
		locked_page_info* bottom = get_bottom_of_locked_pages_stack(locked_pages_stack_p);
		release_lock_on_persistent_page(dam_p, transaction_id, &(bottom->ppage), NONE_OPTION, abort_error);
		pop_bottom_from_locked_pages_stack(locked_pages_stack_p);
	}

	// release allocation for locked_pages_stack
	deinitialize_locked_pages_stack(locked_pages_stack_p);

	// on an abort, overide the result to 0
	if(*abort_error)
		result = 0;

	return result;
}