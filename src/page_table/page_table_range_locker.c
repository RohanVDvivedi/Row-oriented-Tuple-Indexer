#include<page_table_range_locker.h>

#include<persistent_page_functions.h>
#include<page_table_page_util.h>
#include<page_table_page_header.h>

page_table_range_locker* get_new_page_table_range_locker(uint64_t root_page_id, page_table_bucket_range lock_range, int lock_type, const page_table_tuple_defs* pttd_p, const page_access_methods* pam_p, const void* transaction_id, int* abort_error)
{
	// fail if the lock range is invalid
	if(!is_valid_page_table_bucket_range(&lock_range))
		return NULL;

	// TODO
}

int minimize_lock_range_for_page_table_range_locker(page_table_range_locker* ptrl_p, page_table_bucket_range lock_range, const void* transaction_id, int* abort_error)
{
	// fail, if lock_range is invalid OR if the lock_range is not contained within the delegated range of the local_root
	if(!is_valid_page_table_bucket_range(&lock_range) || !is_contained_page_table_bucket_range(&(ptrl_p->delegated_local_root_range), &lock_range))
		return 0;

	// TODO
}

page_table_bucket_range get_lock_range_for_page_table_range_locker(const page_table_range_locker* ptrl_p)
{
	return ptrl_p->delegated_local_root_range;
}

int is_writable_page_table_range_locker(const page_table_range_locker* ptrl_p)
{
	return is_persistent_page_write_locked(&(ptrl_p->local_root));
}

uint64_t get_from_page_table(page_table_range_locker* ptrl_p, uint64_t bucket_id, const void* transaction_id, int* abort_error)
{
	// fail if the bucket_id is not contained within the delegated range of the local_root
	if(!is_bucket_contained_page_table_bucket_range(&(ptrl_p->delegated_local_root_range), bucket_id))
		return ptrl_p->pttd_p->pas_p->NULL_PAGE_ID;

	// TODO
}

int set_in_page_table(page_table_range_locker* ptrl_p, uint64_t bucket_id, uint64_t page_id, const page_modification_methods* pmm_p, const void* transaction_id, int* abort_error)
{
	// fail if the bucket_id is not contained within the delegated range of the local_root
	if(!is_bucket_contained_page_table_bucket_range(&(ptrl_p->delegated_local_root_range), bucket_id))
		return 0;

	// TODO
}

void delete_page_table_range_locker(page_table_range_locker* ptrl_p, const void* transaction_id, int* abort_error);