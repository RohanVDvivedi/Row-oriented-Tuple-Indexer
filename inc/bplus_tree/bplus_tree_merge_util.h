#ifndef BPLUS_TREE_MERGE_UTIL_H
#define BPLUS_TREE_MERGE_UTIL_H

#include<locked_pages_stack.h>
#include<bplus_tree_tuple_definitions.h>
#include<opaque_page_access_methods.h>
#include<opaque_page_modification_methods.h>

// the locked_pages_stack may contain only the pages in the chain that will participate in the merge
// i.e. the root_page may not be in the locked pages stack
// all the locks are released by this function, even on a failure/abort_error
// this function always returns 1, except on an abort_error
int merge_and_unlock_pages_up(uint64_t root_page_id, locked_pages_stack* locked_pages_stack_p, const bplus_tree_tuple_defs* bpttd_p, const page_access_methods* pam_p, const page_modification_methods* pmm_p, const void* transaction_id, int* abort_error);

#endif