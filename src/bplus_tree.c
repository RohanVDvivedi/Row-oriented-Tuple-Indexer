#include<bplus_tree.h>

#include<sorted_packed_page_util.h>

#include<bplus_tree_leaf_page_util.h>
#include<bplus_tree_interior_page_util.h>

#include<storage_capacity_page_util.h>

#include<arraylist.h>

#include<stdlib.h>
#include<string.h>

uint32_t create_new_bplus_tree(const bplus_tree_tuple_defs* bpttds, const data_access_methods* dam_p)
{
	uint32_t root_page_id;
	void* root_page = dam_p->get_new_page_with_write_lock(dam_p->context, &root_page_id);
		if(root_page == NULL)
			return NULL_PAGE_REF;

		init_leaf_page(root_page, dam_p->page_size, bpttds);
	
	dam_p->release_writer_lock_on_page(dam_p->context, root_page);
	
	return root_page_id;
}

const void* find_in_bplus_tree(uint32_t root_id, const void* key, const bplus_tree_tuple_defs* bpttds, const data_access_methods* dam_p)
{
	void* record_found = NULL;

	void* curr_page = dam_p->acquire_page_with_reader_lock(dam_p->context, root_id);

	while(curr_page != NULL)
	{
		switch(get_page_type(curr_page))
		{
			case LEAF_PAGE_TYPE :
			{
				// find record for the given key in b+tree
				uint16_t found_index;
				int found = search_in_sorted_packed_page(curr_page, dam_p->page_size, bpttds->key_def, bpttds->record_def, key, &found_index);

				if(found) // then, copy it to record_found
				{
					const void* record_found_original = get_nth_tuple(curr_page, dam_p->page_size, bpttds->record_def, found_index);
					uint32_t record_found_original_size = get_tuple_size(bpttds->record_def, record_found_original);

					record_found = malloc(record_found_original_size);
					memmove(record_found, record_found_original, record_found_original_size);
				}

				// release lock and cleanup
				dam_p->release_reader_lock_on_page(dam_p->context, curr_page);
				curr_page = NULL;
				break;
			}
			case INTERIOR_PAGE_TYPE :
			{
				// find next page_id to jump to the next level in the b+tree
				int32_t next_indirection_index = find_in_interior_page(curr_page, dam_p->page_size, key, bpttds);
				uint32_t next_page_id = get_index_page_id_from_interior_page(curr_page, dam_p->page_size, next_indirection_index, bpttds);

				// lock next page, prior to releasing the lock on the current page
				void* next_page = dam_p->acquire_page_with_reader_lock(dam_p->context, next_page_id);
				dam_p->release_reader_lock_on_page(dam_p->context, curr_page);

				// reiterate until you reach the leaf page
				curr_page = next_page;
				break;
			}
		}
	}

	return record_found;
}

static uint32_t insert_new_root_node(uint32_t old_root_id, const void* parent_index_insert, const bplus_tree_tuple_defs* bpttds, const data_access_methods* dam_p)
{
	uint32_t new_root_page_id;
	void* new_root_page = dam_p->get_new_page_with_write_lock(dam_p->context, &new_root_page_id);
	init_interior_page(new_root_page, dam_p->page_size, bpttds);

	// insert the only entry for a new root level
	insert_to_sorted_packed_page(new_root_page, dam_p->page_size, bpttds->key_def, bpttds->index_def, parent_index_insert, NULL);

	// update all least referenc of this is page
	set_index_page_id_in_interior_page(new_root_page, dam_p->page_size, -1, bpttds, old_root_id);

	dam_p->release_writer_lock_on_page(dam_p->context, new_root_page);

	return new_root_page_id;
}

int insert_in_bplus_tree(uint32_t* root_id, const void* record, const bplus_tree_tuple_defs* bpttds, const data_access_methods* dam_p)
{
	// record size must be lesser than or equal to half the page size
	if(get_tuple_size(bpttds->record_def, record) >= dam_p->page_size / 2)
		return 0;

	arraylist locked_parents;
	initialize_arraylist(&locked_parents, 64);

	void* curr_page = dam_p->acquire_page_with_writer_lock(dam_p->context, *root_id);

	void* parent_index_insert = NULL;

	int inserted = 0;

	// quit when no locks are held
	while( ! (is_empty_arraylist(&locked_parents) && curr_page == NULL) )
	{
		switch(get_page_type(curr_page))
		{
			case LEAF_PAGE_TYPE :
			{// no duplicates allowed
				// the key should not be present
				int found = search_in_sorted_packed_page(curr_page, dam_p->page_size, bpttds->key_def, bpttds->record_def, record, NULL);

				// try to insert record to the curr_page, only if the key is not found on the page
				if(!found)
					inserted = insert_to_sorted_packed_page(curr_page, dam_p->page_size, bpttds->key_def, bpttds->record_def, record, NULL);
				
				// if inserted or found, then unlock this page and all the parent pages
				if(found || inserted)
				{
					while(!is_empty_arraylist(&locked_parents))
					{
						void* some_parent = (void*) get_front(&locked_parents);
						dam_p->release_writer_lock_on_page(dam_p->context, some_parent);
						pop_front(&locked_parents);
					}

					dam_p->release_writer_lock_on_page(dam_p->context, curr_page);
					curr_page = NULL;
				}
				// insert forcefully with a split
				else if(!found)
				{
					// get new blank page, to split this page into
					uint32_t new_page_id;
					void* new_page = dam_p->get_new_page_with_write_lock(dam_p->context, &new_page_id);
					
					// split and insert to this leaf node and propogate the parent_index_insert, don not forget to set the inserted flag
					parent_index_insert = split_insert_leaf_page(curr_page, record, new_page, new_page_id, dam_p->page_size, bpttds);
					inserted = 1;

					// release lock on both: new and curr page
					dam_p->release_writer_lock_on_page(dam_p->context, new_page);
					dam_p->release_writer_lock_on_page(dam_p->context, curr_page);

					// if there are locked parents, we need to propogate the split
					curr_page = (void*) get_back(&locked_parents);
					pop_back(&locked_parents);
				}
				break;
			}
			case INTERIOR_PAGE_TYPE :
			{
				// we are looping forwards to search for a leaf page to insert this record
				if(parent_index_insert == NULL && !inserted)
				{
					// if the curr page can handle an insert without a split
					// then release locks on all the locked_parents that were locked until now
					// since we won't have to propogate the split
					if(is_page_lesser_than_or_equal_to_half_full(curr_page, dam_p->page_size, bpttds->index_def))
					{
						while(!is_empty_arraylist(&locked_parents))
						{
							void* some_parent = (void*) get_front(&locked_parents);
							dam_p->release_writer_lock_on_page(dam_p->context, some_parent);
							pop_front(&locked_parents);
						}
					}

					// search appropriate indirection page_id from curr_page
					int32_t next_indirection_index = find_in_interior_page(curr_page, dam_p->page_size, record, bpttds);
					uint32_t next_page_id = get_index_page_id_from_interior_page(curr_page, dam_p->page_size, next_indirection_index, bpttds);
					
					// lock this next page
					void* next_page = dam_p->acquire_page_with_writer_lock(dam_p->context, next_page_id);

					// now curr_page is a locked parent of this next_page
					push_back(&locked_parents, curr_page);
					curr_page = next_page;
				}
				// we are looping backwards to propogate the split
				else if(parent_index_insert != NULL && inserted)
				{
					// try to insert record to the curr_page
					int parent_index_inserted = insert_to_sorted_packed_page(curr_page, dam_p->page_size, bpttds->key_def, bpttds->index_def, parent_index_insert, NULL);

					// if insert succeeds, then unlock all pages and quit the loop
					if(parent_index_inserted)
					{
						free(parent_index_insert);
						parent_index_insert = NULL;

						while(!is_empty_arraylist(&locked_parents))
						{
							void* some_parent = (void*) get_front(&locked_parents);
							dam_p->release_writer_lock_on_page(dam_p->context, some_parent);
							pop_front(&locked_parents);
						}

						dam_p->release_writer_lock_on_page(dam_p->context, curr_page);
						curr_page = NULL;
					}
					// else split this interior node to insert
					else
					{
						// get new blank page, to split this page into
						uint32_t new_page_id;
						void* new_page = dam_p->get_new_page_with_write_lock(dam_p->context, &new_page_id);

						// split_insert to this node
						void* next_parent_index_insert = split_insert_interior_page(curr_page, parent_index_insert, new_page, new_page_id, dam_p->page_size, bpttds);
						free(parent_index_insert);
						parent_index_insert = next_parent_index_insert;

						// release lock on both: new and curr page
						dam_p->release_writer_lock_on_page(dam_p->context, new_page);
						dam_p->release_writer_lock_on_page(dam_p->context, curr_page);

						// pop a curr_page (getting immediate parent) to propogate the split
						curr_page = (void*) get_back(&locked_parents);
						pop_back(&locked_parents);
					}
				}
				break;
			}
		}
	}

	// need to insert root to this bplus tree
	if(parent_index_insert != NULL)
	{
		*root_id = insert_new_root_node(*root_id, parent_index_insert, bpttds, dam_p);

		free(parent_index_insert);
		parent_index_insert = NULL;
	}

	deinitialize_arraylist(&locked_parents);

	return inserted;
}

int delete_in_bplus_tree(uint32_t* root, const void* key, const bplus_tree_tuple_defs* bpttds, const data_access_methods* dam_p);

void print_bplus_tree(uint32_t root_id, const bplus_tree_tuple_defs* bpttds, const data_access_methods* dam_p)
{
	void* curr_page = dam_p->acquire_page_with_reader_lock(dam_p->context, root_id);

	switch(get_page_type(curr_page))
	{
		case LEAF_PAGE_TYPE :
		{
			printf("LEAF page_id = %u\n", root_id);
			print_page(curr_page, dam_p->page_size, bpttds->record_def);
			break;
		}
		case INTERIOR_PAGE_TYPE :
		{
			// call print on all its children
			for(int32_t i = -1; i < ((int32_t)(get_index_entry_count_in_interior_page(curr_page))); i++)
			{
				uint32_t child_id = get_index_page_id_from_interior_page(curr_page, dam_p->page_size, i, bpttds);
				print_bplus_tree(child_id, bpttds, dam_p);
			}

			// call print on itself
			printf("INTR page_id = %u\n", root_id);
			print_page(curr_page, dam_p->page_size, bpttds->index_def);

			break;
		}
	}

	dam_p->release_reader_lock_on_page(dam_p->context, curr_page);
}