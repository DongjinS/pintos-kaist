/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"

struct list frame_table;
struct list_elem *start;

static uint64_t page_hash (const struct hash_elem *e, void *aux);
static bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);
static void spt_destructor(struct hash_elem *e, void* aux);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
	start = list_begin(&frame_table); //?
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);
	bool success = false;
	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *new_page = malloc(sizeof(struct page));
		bool (*page_initializer) (struct page *, enum vm_type, void *kva);
		switch (VM_TYPE(type))
		{
			case VM_ANON:
				page_initializer = anon_initializer;
				break;
			case VM_FILE:
				page_initializer = file_backed_initializer;
				break;
		}
		uninit_new(new_page, upage, init, type, aux, page_initializer);

		new_page->writable = writable;
		
		/* TODO: Insert the page into the spt. */

		success = spt_insert_page(spt, new_page);
		ASSERT(success == true);

		return success;
	}
err:
	return success;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page dummy_page;
	dummy_page.va = pg_round_down(va);
	struct hash_elem *e = hash_find(&spt->hash_table, &dummy_page.hash_elem);

	if (e == NULL) {
		return NULL;
	}

	return hash_entry(e, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	if (!hash_insert(&spt->hash_table, &page->hash_elem)) {
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	pml4_clear_page(thread_current()->pml4, page->va);
	hash_delete(&spt->hash_table, &page->hash_elem);

	if (page->frame != NULL) {
		page->frame->page == NULL;
	}
	
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	// clock algorithm
	struct thread *curr = thread_current();
	struct list_elem *e = start;

	for (start = e; start != list_end(&frame_table); start = list_next(&frame_table)){
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va)){
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		}else{
			return victim;
		}
	}

	for (start = list_begin(&frame_table); start != e; start = list_next(&frame_table)){
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va)){
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		}else{
			return victim;
		}
	}

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	ASSERT(victim != NULL);
	if (victim->page != NULL){
		if (swap_out(victim->page)==false){
			PANIC("swap out failed");
		}
	}

	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	
	void *kva = palloc_get_page(PAL_USER);

	if (kva == NULL) {
		// PANIC ("todo: implement eviction");
		frame = vm_evict_frame();
		frame->page = NULL;
		
		return frame;
	}else{
		frame = malloc(sizeof(struct frame));
		if (frame == NULL) {
			PANIC ("todo: handle case when malloc fails.");
		}
		list_push_back(&frame_table ,&frame->frame_elem);
		frame->kva = kva;
		frame->page = NULL;
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static bool
vm_stack_growth (void *addr UNUSED) {
	bool success = vm_alloc_page(VM_ANON | VM_MARKER_STACK, addr, true); // Create uninit page for stack; will become anon page
	if (success == true) {
		thread_current()->user_stack_bottom -= PGSIZE;
	}
	return success;
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	if (is_kernel_vaddr(addr)) {
		return false;
	}

	if (not_present == false) { // when read-only access
		return false;
	}

	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, addr);

	if (page == NULL) {
		void *rsp = user ? f->rsp : thread_current()->user_rsp;
		const int GROWTH_LIMIT = 8;
		const int STACK_LIMIT = USER_STACK - (1<<20);

		// Check stack size max limit and stack growth request heuristically
		if(addr >= STACK_LIMIT && USER_STACK > addr && addr >= rsp - GROWTH_LIMIT){
			void *fpage = thread_current()->user_stack_bottom - PGSIZE;
			if (vm_stack_growth (fpage)) {
				page = spt_find_page(spt, fpage);
				ASSERT(page != NULL);
			} else {
				return false;
			}
		}
		else{
			return false;
		}
	}
	
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = spt_find_page(&thread_current()->spt, va);

	if (page == NULL) {
		return false;
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *t = thread_current();
	ASSERT(pml4_get_page (t->pml4,page->va) == NULL);

	// TODO: pml4_set_page가 false가 뜨는 경우: page table을 위한 물리 메모리가 부족한 경우
	if (!pml4_set_page (t->pml4, page->va, frame->kva, page->writable)){
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->hash_table, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i;
	hash_first(&i, &src->hash_table);
	while (hash_next(&i)) {
		struct page *parent_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = parent_page->operations->type;

		if(type == VM_UNINIT || type == VM_FILE){
			struct uninit_page *uninit = &parent_page->uninit;
			vm_initializer *init = uninit->init;
			struct lazy_load_info *parent_aux = uninit->aux;

			struct lazy_load_info *child_aux = malloc(sizeof(struct lazy_load_info));
			if (child_aux == NULL){
				return false;
			}
			memcpy(child_aux, parent_aux, sizeof(struct lazy_load_info));

			if (!vm_alloc_page_with_initializer(uninit->type, parent_page->va, parent_page->writable, init, child_aux)) {
				return false;
			}
		}
		if(type & VM_ANON == VM_ANON){ // include stack pages
			if (!vm_alloc_page(type, parent_page->va, parent_page->writable)) {
				return false;
			}

			struct page *child_page = spt_find_page(dst, parent_page->va);
			if (!vm_do_claim_page(child_page)) {
				return false;
			}

			ASSERT(parent_page->frame != NULL);
			memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
		}
		if(type == VM_FILE){
		}
		
	}

	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy(&spt->hash_table, spt_destructor);
}

static uint64_t page_hash (const struct hash_elem *e, void *aux)
{
	const struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}

static bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	const struct page *a_ = hash_entry(a, struct page, hash_elem);
	const struct page *b_ = hash_entry(b, struct page, hash_elem);

	return a_->va < b_->va;
}

static void spt_destructor(struct hash_elem *e, void* aux) {
    const struct page *page = hash_entry(e, struct page, hash_elem);
	struct thread *t = thread_current();
	ASSERT(page != NULL);

	if (page_get_type(page) == VM_FILE) {
		if (pml4_is_dirty(t->pml4, page->va)) {
			struct lazy_load_info *aux = page->uninit.aux;
			if (file_write_at(aux->file, page->va, aux->page_read_bytes, aux->ofs) != aux->page_read_bytes) {
				PANIC("writing back to file during munmap failed.");
			}
			pml4_set_dirty(t->pml4, page->va, false);
		}
	}

	spt_remove_page(&t->spt, page);
}