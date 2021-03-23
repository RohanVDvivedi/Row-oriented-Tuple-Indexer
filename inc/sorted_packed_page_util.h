#ifndef SORTED_PACKED_PAGE_UTIL_H
#define SORTED_PACKED_PAGE_UTIL_H

#include<tuple_def.h>

// find using binary search in sorted page
// returns index of the tuple found if, return value == 1
// else returns index to the element just lesser than the the key with return value 0
int search_in_sorted_packed_page(
									const void* page, uint32_t page_size, 
									const tuple_def* key_def, const tuple_def* key_val_def, 
									const void* key, 
									uint16_t* index
								);

// insert tuple
// returns the index of the inserted tuple if, return value == 1
int insert_to_sorted_packed_page(
									void* page, uint32_t page_size, 
									const tuple_def* key_def, const tuple_def* key_val_def, 
									const void* tuple, 
									uint16_t* index
								);

// delete tuple at given index
// returns 1, if the tuple was deleted
int delete_in_sorted_packed_page(
									void* page, uint32_t page_size, 
									const tuple_def* key_val_def, 
									uint16_t index
								);

// insert n tuples from src page to dest page
// returns the number of tuples inserted
uint16_t insert_all_from_sorted_packed_page(
									void* page_dest, const void* page_src, uint32_t page_size, 
									const tuple_def* key_def, const tuple_def* key_val_def, 
									uint16_t start_index, uint16_t end_index
								);

// delete all tuples between index range [start_index, end_index] included
// returns 1, if the tuple was deleted
int delete_all_in_sorted_packed_page(
									void* page, uint32_t page_size, 
									const tuple_def* key_val_def, 
									uint16_t start_index, uint16_t end_index
								);

#endif