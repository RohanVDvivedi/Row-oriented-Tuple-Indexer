#include<bplus_tree_interior_page_header.h>

#include<persistent_page_functions.h>

#include<serial_int.h>

#include<stdlib.h>

// number of bytes in the flags field of the header
#define FLAGS_BYTE_SIZE 1

// bit position of is_last_page_of_level
#define IS_LAST_PAGE_OF_LEVEL_FLAG_POS 0

// number of bytes to store level of the page
// 2 is more than enough for curent computer systems, and allows us to store large amount of data
// values in range 1 to 4 both inclusive
#define BYTES_FOR_PAGE_LEVEL 2

uint32_t get_offset_to_end_of_bplus_tree_interior_page_header(const bplus_tree_tuple_defs* bpttd_p)
{
	return get_offset_to_end_of_common_page_header(&(bpttd_p->pas)) + BYTES_FOR_PAGE_LEVEL + bpttd_p->pas.page_id_width + FLAGS_BYTE_SIZE;
}

uint32_t get_level_of_bplus_tree_interior_page(const persistent_page* ppage, const bplus_tree_tuple_defs* bpttd_p)
{
	return get_bplus_tree_interior_page_header(ppage, bpttd_p).level;
}

uint64_t get_least_keys_page_id_of_bplus_tree_interior_page(const persistent_page* ppage, const bplus_tree_tuple_defs* bpttd_p)
{
	return get_bplus_tree_interior_page_header(ppage, bpttd_p).least_keys_page_id;
}

int is_last_page_of_level_of_bplus_tree_interior_page(const persistent_page* ppage, const bplus_tree_tuple_defs* bpttd_p)
{
	return get_bplus_tree_interior_page_header(ppage, bpttd_p).is_last_page_of_level;
}

static inline uint32_t get_offset_to_bplus_tree_interior_page_header_locals(const bplus_tree_tuple_defs* bpttd_p)
{
	return get_offset_to_end_of_common_page_header(&(bpttd_p->pas));
}

bplus_tree_interior_page_header get_bplus_tree_interior_page_header(const persistent_page* ppage, const bplus_tree_tuple_defs* bpttd_p)
{
	const void* interior_page_header_serial = get_page_header_ua_persistent_page(ppage, bpttd_p->pas.page_size) + get_offset_to_bplus_tree_interior_page_header_locals(bpttd_p);
	return (bplus_tree_interior_page_header){
		.parent = get_common_page_header(ppage, &(bpttd_p->pas)),
		.level = deserialize_uint32(interior_page_header_serial, BYTES_FOR_PAGE_LEVEL),
		.least_keys_page_id = deserialize_uint64(interior_page_header_serial + BYTES_FOR_PAGE_LEVEL, bpttd_p->pas.page_id_width),
		.is_last_page_of_level = ((deserialize_int8(interior_page_header_serial + BYTES_FOR_PAGE_LEVEL + bpttd_p->pas.page_id_width, FLAGS_BYTE_SIZE) >> IS_LAST_PAGE_OF_LEVEL_FLAG_POS) & 1),
	};
}

void serialize_bplus_tree_interior_page_header(void* hdr_serial, const bplus_tree_interior_page_header* bptiph_p, const bplus_tree_tuple_defs* bpttd_p)
{
	serialize_common_page_header(hdr_serial, &(bptiph_p->parent), &(bpttd_p->pas));

	void* bplus_tree_interior_page_header_serial = hdr_serial + get_offset_to_bplus_tree_interior_page_header_locals(bpttd_p);
	serialize_uint32(bplus_tree_interior_page_header_serial, BYTES_FOR_PAGE_LEVEL, bptiph_p->level);
	serialize_uint64(bplus_tree_interior_page_header_serial + BYTES_FOR_PAGE_LEVEL, bpttd_p->pas.page_id_width, bptiph_p->least_keys_page_id);
	serialize_uint64(bplus_tree_interior_page_header_serial + BYTES_FOR_PAGE_LEVEL + bpttd_p->pas.page_id_width, FLAGS_BYTE_SIZE, ((!!(bptiph_p->is_last_page_of_level)) << IS_LAST_PAGE_OF_LEVEL_FLAG_POS));
}

void set_bplus_tree_interior_page_header(persistent_page* ppage, const bplus_tree_interior_page_header* bptiph_p, const bplus_tree_tuple_defs* bpttd_p, const page_modification_methods* pmm_p, const void* transaction_id, int* abort_error)
{
	uint32_t page_header_size = get_page_header_size_persistent_page(ppage, bpttd_p->pas.page_size);

	// allocate memory, to hold complete page_header
	void* hdr_serial = malloc(page_header_size);
	if(hdr_serial == NULL)
		exit(-1);

	// copy the old page_header to it
	memory_move(hdr_serial, get_page_header_ua_persistent_page(ppage, bpttd_p->pas.page_size), page_header_size);

	// serialize bptlph_p on the hdr_serial
	serialize_bplus_tree_interior_page_header(hdr_serial, bptiph_p, bpttd_p);

	// write hdr_serial to the new header position
	set_persistent_page_header(pmm_p, transaction_id, ppage, bpttd_p->pas.page_size, hdr_serial, abort_error);

	// we need to free hdr_serial, even on an abort_error
	free(hdr_serial);
}

#include<stdio.h>

void print_bplus_tree_interior_page_header(const persistent_page* ppage, const bplus_tree_tuple_defs* bpttd_p)
{
	print_common_page_header(ppage, &(bpttd_p->pas));
	printf("level : %"PRIu32"\n", get_level_of_bplus_tree_interior_page(ppage, bpttd_p));
	printf("least_keys_page_id : %"PRIu64"\n", get_least_keys_page_id_of_bplus_tree_interior_page(ppage, bpttd_p));
	printf("is_last_page_of_level : %d\n", is_last_page_of_level_of_bplus_tree_interior_page(ppage, bpttd_p));
}