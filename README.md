# TupleIndexer
A c library that enables you to build data base index structures a b_plus_tree, an array_table, a page_table and a linked_page_list.

A standard bplus_tree (a b+ tree) for your data.

I have come up with another data structure that works like an array, it is very similar to Operating System page tables, but it is optimized to have a compact height and width, by using some clever modifications. It essentially is a loop up table from uint64_t (referred to as bucket_id) to fixed sized tuples.

Similarly a page_table (derived uaing the above mentioned array_table) structure that provides a dynamic mapping between a 64-bit bucket_id to a page_id. It will be further used to support a hash index on top of it.

There is also an implementation of a linked_page_list, this data structure is a doubly-circular linked list of pages, containing tuples.

Together (the page_table and linked_page_list), provides a persistent hash_table data structure.

Finally, there is a sorter, that implements an external merge sort to sort all the tuples, it has optimizations to merge with atmost N runs at a time. It returns a linked page page list of the sorted tuples.

All the above data structures for your data, work over a data store accessible in fixed sized pages (either persistent or non-persistent store) using your own page_access_methods (a struct of functions). It also allows you to implement your own page_modification_methods (again a struct of functions) to intercept calls to page modifications in case if you would like to log the changes to pages, to make them redo-able and undo-able.

Sample implementationd of page_access_methods (unWALed_in_memory_data_store.c) and page_modification_methods (unWALed_page_modification_methods.c) have been provided for reference in src/interface directory.

The Indexes provided (only a b+tree and a page_table (using page_table_range_locker), as of now) are thread safe (using standard latch coupling mechanism), provided correct and api conforming implementation of your own page_access_methods. Yes, page_access_methods struct will be used by TupleIndexer to request access for a page with a READ_LOCK or a WRITE_LOCK, explicitly.

The tuple is laid out as per specifications of TupleStore library.

## Setup instructions
**Install dependencies :**
 * [Cutlery](https://github.com/RohanVDvivedi/Cutlery)
 * [SerializableInteger](https://github.com/RohanVDvivedi/SerializableInteger)
 * [TupleStore](https://github.com/RohanVDvivedi/TupleStore)
 * [ReaderWriterLock](https://github.com/RohanVDvivedi/ReaderWriterLock)

**Download source code :**
 * `git clone https://github.com/RohanVDvivedi/TupleIndexer.git`

**Build from source :**
 * `cd TupleIndexer`
 * `make clean all`

**Install from the build :**
 * `sudo make install`
 * ***Once you have installed from source, you may discard the build by*** `make clean`

## Using The library
 * add `-ltupleindexer -ltuplestore -lrwlock -lserint -lcutlery -lpthread` linker flag, while compiling your application
 * do not forget to include appropriate public api headers as and when needed. this includes
   * `#include<bplus_tree.h>`
   * `#include<page_table.h>`
   * `#include<linked_page_list.h>`
   * `#include<hash_table.h>`
   * `#include<page_access_methods.h>`
   * `#include<page_modification_methods.h>`
 * for testing you may need :
   * `#include<unWALed_in_memory_data_store.h>`
   * `#include<unWALed_page_modification_methods.h>`

## Instructions for uninstalling library

**Uninstall :**
 * `cd TupleIndexer`
 * `sudo make uninstall`
