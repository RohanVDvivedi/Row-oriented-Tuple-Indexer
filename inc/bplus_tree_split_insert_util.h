#ifndef BPLUS_TREE_SPLIT_INSERT_UTIL_H
#define BPLUS_TREE_SPLIT_INSERT_UTIL_H

#include<locked_pages_stack.h>
#include<bplus_tree_tuple_definitions.h>
#include<opaque_data_access_methods.h>
#include<opaque_page_modification_methods.h>

int walk_down_locking_parent_pages_for_split_insert(uint64_t root_page_id, locked_pages_stack* locked_pages_stack_p, const void* record, const bplus_tree_tuple_defs* bpttd_p, const data_access_methods* dam_p);

int split_insert_unlock_pages_up(uint64_t root_page_id, locked_pages_stack* locked_pages_stack_p, const void* record, const bplus_tree_tuple_defs* bpttd_p, const data_access_methods* dam_p, const page_modification_methods* pmm_p);

#endif