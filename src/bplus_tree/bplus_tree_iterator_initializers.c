#include<bplus_tree_iterator.h>

#include<locked_pages_stack.h>
#include<bplus_tree_leaf_page_util.h>
#include<bplus_tree_interior_page_util.h>
#include<bplus_tree_walk_down.h>
#include<sorted_packed_page_util.h>

#include<persistent_page_functions.h>
#include<tuple.h>

#include<find_position.h>

static int adjust_position_for_bplus_tree_iterator(bplus_tree_iterator* bpi_p, const void* key, uint32_t key_element_count_concerned, find_position find_pos, const void* transaction_id, int* abort_error)
{
	// find the leaf_tuple_index for the iterator to start with
	{
		const persistent_page* curr_leaf_page = get_curr_leaf_page(bpi_p);
		bpi_p->curr_tuple_index = 0;

		switch(find_pos)
		{
			case MIN :
			{
				bpi_p->curr_tuple_index = 0;
				break;
			}
			case LESSER_THAN :
			{
				bpi_p->curr_tuple_index = find_preceding_in_sorted_packed_page(
										curr_leaf_page, bpi_p->bpttd_p->pas_p->page_size, 
										bpi_p->bpttd_p->record_def, bpi_p->bpttd_p->key_element_ids, bpi_p->bpttd_p->key_compare_direction, key_element_count_concerned,
										key, bpi_p->bpttd_p->key_def, NULL
									);

				bpi_p->curr_tuple_index = (bpi_p->curr_tuple_index != NO_TUPLE_FOUND) ? bpi_p->curr_tuple_index : 0;
				break;
			}
			case LESSER_THAN_EQUALS :
			{
				bpi_p->curr_tuple_index = find_preceding_equals_in_sorted_packed_page(
										curr_leaf_page, bpi_p->bpttd_p->pas_p->page_size, 
										bpi_p->bpttd_p->record_def, bpi_p->bpttd_p->key_element_ids, bpi_p->bpttd_p->key_compare_direction, key_element_count_concerned,
										key, bpi_p->bpttd_p->key_def, NULL
									);

				bpi_p->curr_tuple_index = (bpi_p->curr_tuple_index != NO_TUPLE_FOUND) ? bpi_p->curr_tuple_index : 0;
				break;
			}
			case GREATER_THAN_EQUALS :
			{
				bpi_p->curr_tuple_index = find_succeeding_equals_in_sorted_packed_page(
										curr_leaf_page, bpi_p->bpttd_p->pas_p->page_size, 
										bpi_p->bpttd_p->record_def, bpi_p->bpttd_p->key_element_ids, bpi_p->bpttd_p->key_compare_direction, key_element_count_concerned,
										key, bpi_p->bpttd_p->key_def, NULL
									);

				bpi_p->curr_tuple_index = (bpi_p->curr_tuple_index != NO_TUPLE_FOUND) ? bpi_p->curr_tuple_index : LAST_TUPLE_INDEX_BPLUS_TREE_LEAF_PAGE;
				break;
			}
			case GREATER_THAN :
			{
				bpi_p->curr_tuple_index = find_succeeding_in_sorted_packed_page(
										curr_leaf_page, bpi_p->bpttd_p->pas_p->page_size, 
										bpi_p->bpttd_p->record_def, bpi_p->bpttd_p->key_element_ids, bpi_p->bpttd_p->key_compare_direction, key_element_count_concerned,
										key, bpi_p->bpttd_p->key_def, NULL
									);

				bpi_p->curr_tuple_index = (bpi_p->curr_tuple_index != NO_TUPLE_FOUND) ? bpi_p->curr_tuple_index : LAST_TUPLE_INDEX_BPLUS_TREE_LEAF_PAGE;
				break;
			}
			case MAX :
			{
				uint32_t tuple_count_on_curr_leaf_page = get_tuple_count_on_persistent_page(curr_leaf_page, bpi_p->bpttd_p->pas_p->page_size, &(bpi_p->bpttd_p->record_def->size_def));
				if(tuple_count_on_curr_leaf_page == 0)
					bpi_p->curr_tuple_index = 0;
				else
					bpi_p->curr_tuple_index = tuple_count_on_curr_leaf_page - 1;
				break;
			}
		}
	}

	// if the bplus_tree is itself empty then nothing needs to be done any further
	if(is_empty_bplus_tree(bpi_p))
		return 1;

	// iterate next or previous in bplus_tree_iterator, based on the find_pos
	// this is not required for MIN and MAX
	switch(find_pos)
	{
		case LESSER_THAN :
		{
			const void* tuple_to_skip = get_tuple_bplus_tree_iterator(bpi_p);
			while(tuple_to_skip != NULL && compare_tuples(tuple_to_skip, bpi_p->bpttd_p->record_def, bpi_p->bpttd_p->key_element_ids, key, bpi_p->bpttd_p->key_def, NULL, bpi_p->bpttd_p->key_compare_direction, key_element_count_concerned) >= 0)
			{
				prev_bplus_tree_iterator(bpi_p, transaction_id, abort_error);
				if(*abort_error)
				{
					delete_bplus_tree_iterator(bpi_p, transaction_id, abort_error);
					return 0;
				}
				tuple_to_skip = get_tuple_bplus_tree_iterator(bpi_p);
			}
			break;
		}
		case LESSER_THAN_EQUALS :
		{
			const void* tuple_to_skip = get_tuple_bplus_tree_iterator(bpi_p);
			while(tuple_to_skip != NULL && compare_tuples(tuple_to_skip, bpi_p->bpttd_p->record_def, bpi_p->bpttd_p->key_element_ids, key, bpi_p->bpttd_p->key_def, NULL, bpi_p->bpttd_p->key_compare_direction, key_element_count_concerned) > 0)
			{
				prev_bplus_tree_iterator(bpi_p, transaction_id, abort_error);
				if(*abort_error)
				{
					delete_bplus_tree_iterator(bpi_p, transaction_id, abort_error);
					return 0;
				}
				tuple_to_skip = get_tuple_bplus_tree_iterator(bpi_p);
			}
			break;
		}
		case GREATER_THAN_EQUALS :
		{
			const void* tuple_to_skip = get_tuple_bplus_tree_iterator(bpi_p);
			while(tuple_to_skip != NULL && compare_tuples(tuple_to_skip, bpi_p->bpttd_p->record_def, bpi_p->bpttd_p->key_element_ids, key, bpi_p->bpttd_p->key_def, NULL, bpi_p->bpttd_p->key_compare_direction, key_element_count_concerned) < 0)
			{
				next_bplus_tree_iterator(bpi_p, transaction_id, abort_error);
				if(*abort_error)
				{
					delete_bplus_tree_iterator(bpi_p, transaction_id, abort_error);
					return 0;
				}
				tuple_to_skip = get_tuple_bplus_tree_iterator(bpi_p);
			}
			break;
		}
		case GREATER_THAN :
		{
			const void* tuple_to_skip = get_tuple_bplus_tree_iterator(bpi_p);
			while(tuple_to_skip != NULL && compare_tuples(tuple_to_skip, bpi_p->bpttd_p->record_def, bpi_p->bpttd_p->key_element_ids, key, bpi_p->bpttd_p->key_def, NULL, bpi_p->bpttd_p->key_compare_direction, key_element_count_concerned) <= 0)
			{
				next_bplus_tree_iterator(bpi_p, transaction_id, abort_error);
				if(*abort_error)
				{
					delete_bplus_tree_iterator(bpi_p, transaction_id, abort_error);
					return 0;
				}
				tuple_to_skip = get_tuple_bplus_tree_iterator(bpi_p);
			}
			break;
		}
		default :
		{
			break;
		}
	}

	return 1;
}

#include<bplus_tree_walk_down.h>

int initialize_bplus_tree_unstacked_iterator(bplus_tree_iterator* bpi_p, uint64_t root_page_id, locked_pages_stack lps, const void* key, uint32_t key_element_count_concerned, find_position find_pos, int lock_type, const bplus_tree_tuple_defs* bpttd_p, const page_access_methods* pam_p, const page_modification_methods* pmm_p, const void* transaction_id, int* abort_error)
{
	// the following 2 must be present
	if(bpttd_p == NULL || pam_p == NULL)
		return 0;

	// TODO
}

int initialize_bplus_tree_unstacked_iterator(bplus_tree_iterator* bpi_p, uint64_t root_page_id, const void* key, uint32_t key_element_count_concerned, find_position find_pos, int lock_type, const bplus_tree_tuple_defs* bpttd_p, const page_access_methods* pam_p, const page_modification_methods* pmm_p, const void* transaction_id, int* abort_error)
{
	// the following 2 must be present
	if(bpttd_p == NULL || pam_p == NULL)
		return 0;

	bpi_p->is_stacked = 0;
	bpi_p->bpttd_p = bpttd_p;
	bpi_p->pam_p = pam_p;
	bpi_p->pmm_p = pmm_p;

	bpi_p->curr_page = walk_down_for_iterator_using_key(root_page_id, key, key_element_count_concerned, find_pos, lock_type, bpttd_p, pam_p, transaction_id, abort_error);
	if(*abort_error)
		return 0;

	return adjust_position_for_bplus_tree_iterator(bpi_p, key, key_element_count_concerned, find_pos, transaction_id, abort_error);
}