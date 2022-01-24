/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	// struct file_page *file_page UNUSED = &page->file;

	if (page == NULL){
		return false;
	}
	struct lazy_load_info *lazy_load_info = page->uninit.aux;
	struct file *file = lazy_load_info->file;
	off_t ofs = lazy_load_info->ofs;
	size_t page_read_bytes = lazy_load_info->page_read_bytes;
	size_t page_zero_bytes = lazy_load_info->page_zero_bytes;

	file_seek(file, ofs);
	ASSERT(page->frame != NULL);
	if (file_read(file, kva, page_read_bytes) != (int) page_read_bytes) {
		return false;
	}
	memset(kva + page_read_bytes, 0, page_zero_bytes);

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	// struct file_page *file_page UNUSED = &page->file;
	struct thread *t = thread_current();

	if(page == NULL){
		return false;
	}
	
	struct lazy_load_info *aux = page->uninit.aux;
	if (pml4_is_dirty(t->pml4, page->va)) {
		if (file_write_at(aux->file, page->va, aux->page_read_bytes, aux->ofs) != (off_t)aux->page_read_bytes) {
			PANIC("writing back to file during munmap failed.");
		}
		pml4_set_dirty(t->pml4, page->va, false);
	}
	// spt_remove_page(&t->spt, page);
	pml4_clear_page(t->pml4, page->va);
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	// struct file_page *file_page UNUSED = &page->file;
	struct uninit_page *uninit UNUSED = &page->uninit;
	struct lazy_load_info *aux = (struct lazy_load_info *)(uninit->aux);

	free(aux);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	struct file *m_file = file_reopen(file);
	struct thread *t = thread_current();
	
	void *original_addr = addr;
	struct page *page;
	size_t read_bytes = length > file_length(m_file) ? file_length(m_file) : length;
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

	while (read_bytes > 0 || zero_bytes > 0){
		if (spt_find_page(&t->spt, addr) != NULL){
			while (original_addr < addr) {
				page = spt_find_page(&t->spt, original_addr);
				spt_remove_page(&t->spt, page);
				original_addr += PGSIZE;
			}
			return NULL;
		}

		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct lazy_load_info *aux = malloc(sizeof(struct lazy_load_info));
		ASSERT(aux != NULL);
		aux->file = m_file;
		aux->ofs = offset;
		aux->page_read_bytes = page_read_bytes;
		aux->page_zero_bytes = page_zero_bytes;
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux)) {
			free(aux);
			while (original_addr < addr) {
				page = spt_find_page(&t->spt, original_addr);
				spt_remove_page(&t->spt, page);
				original_addr += PGSIZE;
			}
			return NULL;
		}

		offset += page_read_bytes;
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
	}
	return original_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	// TODO: 다음 교과서 내용 확인 못했음
	// Unmaps the mapping for the specified address range addr, 
	// which must be the virtual address returned by a previous call to mmap by the same process that has not yet been unmapped.
	
	struct thread *t = thread_current();
	struct page *page;

	if ((page = spt_find_page(&t->spt, addr)) == NULL) {
		return;
	}

	struct file *file = ((struct lazy_load_info *)page->uninit.aux)->file;

	while (page != NULL && page_get_type(page) == VM_FILE) {
		if (pml4_is_dirty(t->pml4, page->va)) {
			struct lazy_load_info *aux = page->uninit.aux;
			if (file_write_at(aux->file, page->va, aux->page_read_bytes, aux->ofs) != aux->page_read_bytes) {
				PANIC("writing back to file during munmap failed.");
			}
			pml4_set_dirty(t->pml4, page->va, false);
		}
		spt_remove_page(&t->spt, page);  // pml4_clear() ?
		addr += PGSIZE;
		page = spt_find_page(&t->spt, addr);
	}

	file_close(file);
}
