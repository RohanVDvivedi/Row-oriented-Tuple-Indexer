#ifndef PERSISTENT_PAGE_H
#define PERSISTENT_PAGE_H

typedef struct persistent_page persistent_page;
struct persistent_page
{
	// id of the page
	uint64_t page_id;

	// page itself
	void* page;
};

#define get_persistent_page(page_id_v, page_v) ((const persistent_page){.page_id = page_id_v, .page = page_v})

#endif