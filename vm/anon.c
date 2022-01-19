/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
/* ------------- project 3 ----------- */
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"
#define CEILING(x, y) (((x) + (y) - 1) / (y))
#define SECTORS_PER_PAGE CEILING(PGSIZE, DISK_SECTOR_SIZE)
/* ----------------------------------- */

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* ------------- project 3 ----------- */
static struct bitmap *swap_table;
/* ----------------------------------- */

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	/* ------------- project 3 ----------- */
	// swap_disk = disk_get(1,1);
	swap_disk = NULL;

	// disk_sector_t bitcnt = disk_size(swap_disk)/SECTORS_PER_PAGE;
	// swap_table = bitmap_create(bitcnt);
	/* ----------------------------------- */
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
