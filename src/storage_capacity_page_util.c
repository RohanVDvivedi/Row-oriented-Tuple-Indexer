#include<storage_capacity_page_util.h>

#include<page_layout.h>
#include<tuple.h>

int is_page_lesser_than_half_full(const void* page, uint32_t page_size, const tuple_def* def)
{
	uint32_t allotted_space = get_space_allotted_to_all_tuples(page, page_size, def);
	uint32_t used_space = get_space_occupied_by_all_tuples(page, page_size, def);
	return used_space < (allotted_space / 2);
}

int is_page_lesser_than_or_equal_to_half_full(const void* page, uint32_t page_size, const tuple_def* def)
{
	uint32_t allotted_space = get_space_allotted_to_all_tuples(page, page_size, def);
	uint32_t used_space = get_space_occupied_by_all_tuples(page, page_size, def);
	return used_space <= (allotted_space / 2);
}

int is_page_more_than_or_equal_to_half_full(const void* page, uint32_t page_size, const tuple_def* def)
{
	uint32_t allotted_space = get_space_allotted_to_all_tuples(page, page_size, def);
	uint32_t used_space = get_space_occupied_by_all_tuples(page, page_size, def);
	return used_space >= (allotted_space / 2);
}

int is_page_more_than_half_full(const void* page, uint32_t page_size, const tuple_def* def)
{
	uint32_t allotted_space = get_space_allotted_to_all_tuples(page, page_size, def);
	uint32_t used_space = get_space_occupied_by_all_tuples(page, page_size, def);
	return used_space > (allotted_space / 2);
}

int can_insert_this_tuple_without_split_for_bplus_tree(const void* page, uint32_t page_size, const tuple_def* def, const void* tuple)
{
	uint32_t allotted_space = get_space_allotted_to_all_tuples(page, page_size, def);
	uint32_t used_space = get_space_occupied_by_all_tuples(page, page_size, def);

	uint32_t space_required_by_tuple = get_tuple_size(def, tuple) + get_additional_space_overhead_per_tuple(page_size, def);

	if(allotted_space >= used_space + space_required_by_tuple)
		return 1;

	return 0;
}

int may_require_split_for_insert_for_bplus_tree(const void* page, uint32_t page_size, const tuple_def* def)
{
	uint32_t allotted_space = get_space_allotted_to_all_tuples(page, page_size, def);
	uint32_t used_space = get_space_occupied_by_all_tuples(page, page_size, def);
	
	// page will be surely split if the unused space is lesser than incoming tuple size for fixed sized tuple
	if(is_fixed_sized_tuple_def(def))
		return (allotted_space - used_space) < def->size;
	// for a variable sized tuple the page must be more than half full to may require spliting
	else
		return used_space > (allotted_space / 2);
}

int may_require_merge_or_redistribution_for_delete_for_bplus_tree(const void* page, uint32_t page_size, const tuple_def* def, uint32_t deletion_index)
{
	uint32_t allotted_space = get_space_allotted_to_all_tuples(page, page_size, def);
	uint32_t used_space = get_space_occupied_by_all_tuples(page, page_size, def);

	// if the page is already lesser than half full, then ti may require merging or redistribution
	if(used_space < (allotted_space / 2))
		return 1;

	uint32_t new_used_space = used_space;

	// find new used space after deleting the tuple at the current index (and after full compaction and tomb stone removal)
	if(deletion_index < get_tuple_count(page, page_size, def))
		new_used_space -= get_space_occupied_by_tuples(page, page_size, def, deletion_index, deletion_index);

	// if the new used space is lesser than half full then it may required merging/redistribution
	return new_used_space < (allotted_space / 2);
}