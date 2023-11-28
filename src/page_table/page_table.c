#include<page_table.h>

#include<page_table_page_util.h>
#include<page_table_page_header.h>
#include<persistent_page_functions.h>
#include<locked_pages_stack.h>

uint64_t get_new_page_table(const page_table_tuple_defs* pttd_p, const page_access_methods* pam_p, const page_modification_methods* pmm_p, const void* transaction_id, int* abort_error)
{
	persistent_page root_page = get_new_persistent_page_with_write_lock(pam_p, transaction_id, abort_error);

	// failure to acquire a new page
	if(*abort_error)
		return pttd_p->pas_p->NULL_PAGE_ID;

	// init root page as if it was the first leaf page
	init_page_table_page(&root_page, 0, 0, pttd_p, pmm_p, transaction_id, abort_error);
	if(*abort_error)
	{
		release_lock_on_persistent_page(pam_p, transaction_id, &root_page, NONE_OPTION, abort_error);
		return pttd_p->pas_p->NULL_PAGE_ID;
	}

	uint64_t res = root_page.page_id;
	release_lock_on_persistent_page(pam_p, transaction_id, &root_page, NONE_OPTION, abort_error);
	if(*abort_error)
		return pttd_p->pas_p->NULL_PAGE_ID;

	return res;
}

int destroy_page_table(uint64_t root_page_id, const page_table_tuple_defs* pttd_p, const page_access_methods* pam_p, const void* transaction_id, int* abort_error)
{
	// create a stack of capacity = max_page_table_height
	locked_pages_stack* locked_pages_stack_p = &((locked_pages_stack){});
	initialize_locked_pages_stack(locked_pages_stack_p, pttd_p->max_page_table_height);

	{
		// get lock on the root page of the page_table
		persistent_page root_page = acquire_persistent_page_with_lock(pam_p, transaction_id, root_page_id, READ_LOCK, abort_error);
		if(*abort_error)
			goto ABORT_ERROR;

		// push the root page onto the stack
		push_to_locked_pages_stack(locked_pages_stack_p, &INIT_LOCKED_PAGE_INFO(root_page, 0));
	}

	while(get_element_count_locked_pages_stack(locked_pages_stack_p) > 0)
	{
		locked_page_info* curr_locked_page = get_top_of_locked_pages_stack(locked_pages_stack_p);

		// on leaf just free the leaf
		if(is_page_table_leaf_page(&(curr_locked_page->ppage), pttd_p))
		{
			// free it and pop it from the stack
			release_lock_on_persistent_page(pam_p, transaction_id, &(curr_locked_page->ppage), FREE_PAGE, abort_error);
			if(*abort_error)
				goto ABORT_ERROR;
			pop_from_locked_pages_stack(locked_pages_stack_p);
		}
		// handle free-ing for interior pages
		else
		{
			// get tuple_count of the page
			uint32_t tuple_count = get_tuple_count_on_persistent_page(&(curr_locked_page->ppage), pttd_p->pas_p->page_size, &(pttd_p->entry_def->size_def));

			int pushed_child = 0;

			// if child index is lesser than tuple_count
			while(curr_locked_page->child_index < tuple_count)
			{
				// then push it's child at child_index onto the stack (with child_index = 0), while incrementing its child index
				uint64_t child_page_id = get_child_page_id_at_child_index_in_page_table_page(&(curr_locked_page->ppage), curr_locked_page->child_index++, pttd_p);

				// if it is a NULL_PAGE_ID, then continue
				if(child_page_id == pttd_p->pas_p->NULL_PAGE_ID)
					continue;

				persistent_page child_page = acquire_persistent_page_with_lock(pam_p, transaction_id, child_page_id, READ_LOCK, abort_error);
				if(*abort_error)
					goto ABORT_ERROR;

				push_to_locked_pages_stack(locked_pages_stack_p, &INIT_LOCKED_PAGE_INFO(child_page, 0));

				pushed_child = 1;
			}

			// we have freed all its children, now we can free this page
			if(pushed_child == 0 && curr_locked_page->child_index == tuple_count)
			{
				// free it and pop it from the stack
				release_lock_on_persistent_page(pam_p, transaction_id, &(curr_locked_page->ppage), FREE_PAGE, abort_error);
				if(*abort_error)
					goto ABORT_ERROR;
				pop_from_locked_pages_stack(locked_pages_stack_p);
			}
		}
	}

	ABORT_ERROR:;
	// release locks on all the pages, we had locks on until now
	while(get_element_count_locked_pages_stack(locked_pages_stack_p) > 0)
	{
		locked_page_info* bottom = get_bottom_of_locked_pages_stack(locked_pages_stack_p);
		release_lock_on_persistent_page(pam_p, transaction_id, &(bottom->ppage), NONE_OPTION, abort_error);
		pop_bottom_from_locked_pages_stack(locked_pages_stack_p);
	}

	deinitialize_locked_pages_stack(locked_pages_stack_p);

	return 1;
}

void print_page_table(uint64_t root_page_id, int only_leaf_pages, const page_table_tuple_defs* pttd_p, const page_access_methods* pam_p, const void* transaction_id, int* abort_error)
{
	// print the root page id of the page_table
	printf("\n\nPage_table @ root_page_id = %"PRIu64"\n\n", root_page_id);

	// create a stack of capacity = max_page_table_height
	locked_pages_stack* locked_pages_stack_p = &((locked_pages_stack){});
	initialize_locked_pages_stack(locked_pages_stack_p, pttd_p->max_page_table_height);

	{
		// get lock on the root page of the page_table
		persistent_page root_page = acquire_persistent_page_with_lock(pam_p, transaction_id, root_page_id, READ_LOCK, abort_error);
		if(*abort_error)
			goto ABORT_ERROR;

		// push the root page onto the stack
		push_to_locked_pages_stack(locked_pages_stack_p, &INIT_LOCKED_PAGE_INFO(root_page, 0));
	}

	while(get_element_count_locked_pages_stack(locked_pages_stack_p) > 0)
	{
		locked_page_info* curr_locked_page = get_top_of_locked_pages_stack(locked_pages_stack_p);

		// print current page as a leaf page
		if(is_page_table_leaf_page(&(curr_locked_page->ppage), pttd_p))
		{
			// print this page and its page_id
			printf("page_id : %"PRIu64"\n\n", curr_locked_page->ppage.page_id);
			print_page_table_page(&(curr_locked_page->ppage), pttd_p);
			printf("xxxxxxxxxxxxx\n\n");

			// unlock it and pop it from the stack
			release_lock_on_persistent_page(pam_p, transaction_id, &(curr_locked_page->ppage), NONE_OPTION, abort_error);
			pop_from_locked_pages_stack(locked_pages_stack_p);
			if(*abort_error)
				goto ABORT_ERROR;
		}
		// print current page as an interior page
		else
		{
			// get tuple_count of the page
			uint32_t tuple_count = get_tuple_count_on_persistent_page(&(curr_locked_page->ppage), pttd_p->pas_p->page_size, &(pttd_p->entry_def->size_def));

			int pushed_child = 0;

			// if child index is lesser than tuple_count
			while(curr_locked_page->child_index < tuple_count)
			{
				// then push it's child at child_index onto the stack (with child_index = 0), while incrementing its child index
				uint64_t child_page_id = get_child_page_id_at_child_index_in_page_table_page(&(curr_locked_page->ppage), curr_locked_page->child_index++, pttd_p);

				// if it is a NULL_PAGE_ID, then continue
				if(child_page_id == pttd_p->pas_p->NULL_PAGE_ID)
					continue;

				persistent_page child_page = acquire_persistent_page_with_lock(pam_p, transaction_id, child_page_id, READ_LOCK, abort_error);
				if(*abort_error)
					goto ABORT_ERROR;

				push_to_locked_pages_stack(locked_pages_stack_p, &INIT_LOCKED_PAGE_INFO(child_page, 0));

				pushed_child = 1;
			}

			// we have printed all its children, now we print this page
			if(pushed_child == 0 && curr_locked_page->child_index == tuple_count)
			{
				if(!only_leaf_pages)
				{
					// print this page and its page_id
					printf("page_id : %"PRIu64"\n\n", curr_locked_page->ppage.page_id);
					print_page_table_page(&(curr_locked_page->ppage), pttd_p);
					printf("xxxxxxxxxxxxx\n");
				}

				// pop it from the stack and unlock it
				release_lock_on_persistent_page(pam_p, transaction_id, &(curr_locked_page->ppage), NONE_OPTION, abort_error);
				pop_from_locked_pages_stack(locked_pages_stack_p);
				if(*abort_error)
					goto ABORT_ERROR;
			}
		}
	}

	ABORT_ERROR:;
	// release locks on all the pages, we had locks on until now
	while(get_element_count_locked_pages_stack(locked_pages_stack_p) > 0)
	{
		locked_page_info* bottom = get_bottom_of_locked_pages_stack(locked_pages_stack_p);
		release_lock_on_persistent_page(pam_p, transaction_id, &(bottom->ppage), NONE_OPTION, abort_error);
		pop_bottom_from_locked_pages_stack(locked_pages_stack_p);
	}

	deinitialize_locked_pages_stack(locked_pages_stack_p);
}