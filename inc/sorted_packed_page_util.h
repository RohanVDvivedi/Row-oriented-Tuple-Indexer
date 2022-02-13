#ifndef SORTED_PACKED_PAGE_UTIL_H
#define SORTED_PACKED_PAGE_UTIL_H

#include<tuple_def.h>

// returns index at which a new tuple may be inserted
uint32_t find_insertion_point_in_sorted_packed_page(
									const void* page, uint32_t page_size, 
									const tuple_def* tpl_def, uint32_t* keys_to_compare, uint32_t keys_count,
									const void* tuple
									);

// insert tuple
// returns 1 tuple was inserted
// returns the index the new tuple is inserted on
int insert_to_sorted_packed_page(
									void* page, uint32_t page_size, 
									const tuple_def* tpl_def, uint32_t* keys_to_compare, uint32_t keys_count,
									const void* tuple, 
									uint32_t* index
								);

// delete tuple at given index
// returns 1, if the tuple was deleted
int delete_in_sorted_packed_page(
									void* page, uint32_t page_size, 
									const tuple_def* tpl_def, 
									uint32_t index
								);

// insert n tuples from src page to dest page
// returns the number of tuples inserted
uint32_t insert_all_from_sorted_packed_page(
									void* page_dest, const void* page_src, uint32_t page_size, 
									const tuple_def* tpl_def, uint32_t* keys_to_compare, uint32_t keys_count,
									uint32_t start_index, uint32_t end_index
								);

// delete all tuples between index range [start_index, end_index] included
// returns 1, if the tuple was deleted
int delete_all_in_sorted_packed_page(
									void* page, uint32_t page_size, 
									const tuple_def* tpl_def, 
									uint32_t start_index, uint32_t end_index
								);

#define NOT_FOUND (~((uint32_t)0))

// returns index of the tuple found
uint32_t search_in_sorted_packed_page(
									const void* page, uint32_t page_size, 
									const tuple_def* tpl_def, uint32_t* keys_to_compare, uint32_t keys_count,
									const void* tuple
								);
#endif