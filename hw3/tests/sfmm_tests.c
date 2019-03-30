#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"

#define MIN_BLOCK_SIZE (32)

/*
 * Assert the total number of free blocks of a specified size,
 * including quick lists.  If size == 0, then assert the total number
 * of all free blocks.  Note that blocks in quick lists are still marked
 * as allocated, even though they are technically free.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    sf_block *bp = sf_free_list_head.body.links.next;
    while(bp != &sf_free_list_head) {
			if(size == 0 || size == (bp->header.block_size & BLOCK_SIZE_MASK))
				cnt++;
			bp = bp->body.links.next;
    }
    for(int n = 0; n < NUM_QUICK_LISTS; n++) {
			if(size == 0 || size == (n << 4) + MIN_BLOCK_SIZE)
				cnt += sf_quick_lists[n].length;
    }
    cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)", count, cnt);
}

/*
 * Assert the total number of blocks in a single quick list (if size > 0),
 * or in all quick lists (if size == 0).
 */
void assert_quick_list_block_count(size_t size, int count) {
    int n = -1;
    int cnt = 0;
    if(size > 0)
		n = (size - MIN_BLOCK_SIZE) >> 4;
    if(n >= 0) {
		cr_assert_eq(sf_quick_lists[n].length, count,
				"Wrong number of blocks in quick list %d (exp=%d, found=%d)",
				n, count, sf_quick_lists[n].length);
    } else {
		for(n = 0; n < NUM_QUICK_LISTS; n++)
			cnt += sf_quick_lists[n].length;
		cr_assert_eq(cnt, count, "Wrong number of blocks in quick lists (exp=%d, found=%d)",
			count, cnt);
    }
}

Test(sf_memsuite_student, malloc_an_Integer_check_freelist, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	int *x = sf_malloc(sizeof(int));

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);
	assert_quick_list_block_count(0, 0);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sf_memsuite_student, malloc_three_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(3 * PAGE_SZ - sizeof(sf_prologue) - sizeof(sf_epilogue) - MIN_BLOCK_SIZE);

	cr_assert_not_null(x, "x is NULL!");
	assert_free_block_count(0, 0);
	assert_quick_list_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sf_memsuite_student, malloc_over_four_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(PAGE_SZ << 2);

	cr_assert_null(x, "x is not NULL!");
	assert_free_block_count(0, 1);
	assert_free_block_count(16336, 1);
	assert_quick_list_block_count(0, 0);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sf_memsuite_student, free_quick, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(32);
	/* void *z = */ sf_malloc(1);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(3936, 1);
	assert_quick_list_block_count(48, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_no_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(1);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(208, 1);
	assert_free_block_count(3776, 1);
	assert_quick_list_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *w = */ sf_malloc(8);
	void *x = sf_malloc(200);
	void *y = sf_malloc(300);
	/* void *z = */ sf_malloc(4);

	sf_free(y);
	sf_free(x);

	assert_free_block_count(0, 2);
	assert_free_block_count(528, 1);
	assert_free_block_count(3456, 1);
	assert_quick_list_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, freelist, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *u = sf_malloc(200);
	/* void *v = */ sf_malloc(300);
	void *w = sf_malloc(400);
	/* void *x = */ sf_malloc(500);
	void *y = sf_malloc(600);
	/* void *z = */ sf_malloc(700);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_free_block_count(0, 4);
	assert_free_block_count(208, 1);
	assert_free_block_count(416, 1);
	assert_free_block_count(608, 1);
	assert_free_block_count(1264, 1);
	assert_quick_list_block_count(0, 0);

	// First block in list should be the most recently freed block.
	sf_block *bp = sf_free_list_head.body.links.next;
	cr_assert_eq(bp, (sf_header *)((char *)y - sizeof(sf_header)),
		     "Wrong first block in main free list: (found=%p, exp=%p)",
                     bp, (sf_header *)((char *)y - sizeof(sf_header)));
}

Test(sf_memsuite_student, realloc_larger_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int));
	/* void *y = */ sf_malloc(10);
	x = sf_realloc(x, sizeof(int) * 10);

	cr_assert_not_null(x, "x is NULL!");
	sf_header *hp = (sf_header *)((char *)x - sizeof(sf_header));
	cr_assert(hp->block_size & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((hp->block_size & BLOCK_SIZE_MASK) == 48, "Realloc'ed block size not what was expected!");

	assert_free_block_count(0, 2);
	assert_free_block_count(3936, 1);
	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(32, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_splinter, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int) * 8);
	void *y = sf_realloc(x, sizeof(char));

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_header *hp = (sf_header *)((char*)y - sizeof(sf_header));
	cr_assert(hp->block_size & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((hp->block_size & BLOCK_SIZE_MASK) == 48, "Block size not what was expected!");
	cr_assert(hp->requested_size == 1, "Requested size not what was expected!");

	// There should be only one free block of size 4000.
	assert_free_block_count(0, 1);
	assert_free_block_count(4000, 1);
	assert_quick_list_block_count(0, 0);
}

Test(sf_memsuite_student, realloc_smaller_block_free_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(double) * 8);
	void *y = sf_realloc(x, sizeof(int));

	cr_assert_not_null(y, "y is NULL!");

	sf_header *hp = (sf_header *)((char*)y - sizeof(sf_header));
	cr_assert(hp->block_size & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((hp->block_size & BLOCK_SIZE_MASK) == 32, "Realloc'ed block size not what was expected!");
	cr_assert(hp->requested_size == 4, "Requested size not what was expected!");

	// After realloc'ing x, we can return a block of size 48 to the freelist.
	// This block will go into the main freelist and be coalesced, as we do not add
	// remainder blocks to a quick list.
	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);
	assert_quick_list_block_count(0, 0);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

// /* Basic Allocating */
// Test() {

// }

/* Test Allocating Multiple Pages & Coalescing */
// Test(sf_memsuite_student, multi_page_alloc, .init = sf_mem_init, .fini = sf_mem_fini) {
// 	void *ptr = sf_malloc(8116);

//     *ptr = 

//     // sf_show_heap();
// }

// /* Test Block Splitting */
// Test(sf_memsuite_student) {
// 	void *ptr = sf_malloc(32);

//     *ptr = 

//     // sf_show_heap();
// }

/* Test No Coalescing When No Free Adjacent Blocks */

/* Test Invalid Free Pointers */

/* Test equal data after Realloc */

/* Realloc to same size */