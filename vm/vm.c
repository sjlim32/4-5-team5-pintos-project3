/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/mmu.h"
#include "threads/vaddr.h"

#include "lib/kernel/hash.h"
#include "include/userprog/process.h"
#include <string.h>

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
}

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

static void hash_copy_action (struct hash_elem *, void *);

bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table 	*spt = &thread_current ()->spt;
	struct page						*page = malloc(sizeof(struct page));
	void 							*initializer;
	bool							succ = false;

	type = (enum vm_type)VM_TYPE(type);

	if(!page)	goto err;

	if (spt_find_page (spt, upage) == NULL) 
	{
		switch(type)
		{
			case VM_ANON:
				initializer = anon_initializer;
				break;

			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}

		if(writable)
			upage = (uint64_t)upage | PTE_W;

		uninit_new(page, upage, init, type, aux, initializer);

		if(spt_insert_page(spt, page))
			return true;
		else
			goto err;
	}
	else
		goto err;

err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page 		*page = NULL;

	if(hash_empty(&spt->spt_hash))
		return page;

	page = page_lookup(va, spt);

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	int succ = false;
	struct hash_elem *result_elem;
	/* TODO: Fill this function. */
	result_elem = hash_insert(&spt->spt_hash, &page->hash_elem);

	if(result_elem == NULL)
		succ = true;

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;

	/* TODO: Fill this function. */
	frame = (struct frame *)calloc(sizeof(struct frame), 1);
	
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);

	if(!frame->kva)	PANIC("ToDo");				// swap function

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr)
{
	struct supplemental_page_table	*spt = &thread_current ()->spt;
	void							*address = pg_round_down (addr);
	struct page						*p = spt_find_page (spt, address);

	while (!p)
	{
		vm_alloc_page (VM_ANON | IS_STACK, address, true);
		address += PGSIZE;
		p = spt_find_page (spt, address);
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr, bool user, bool write, bool not_present) {
	uintptr_t rsp = f->rsp;

	bool addr_in_stack = ((uint64_t)addr >= (rsp - 8)) && (USER_STACK - (uint64_t)addr < (1 << 20));
	if (addr_in_stack) {
		vm_stack_growth (addr);
	}

	return vm_claim_page (addr);
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
vm_claim_page (void *va) {
	struct page *page = NULL;

	page = spt_find_page(&thread_current()->spt, va);

	if(page == NULL)
		return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame 	*frame = vm_get_frame ();
	struct thread 	*t = thread_current ();
	bool writable;

	frame->page = page;
	page->frame = frame;

	writable = (uint64_t)page->va & PTE_W;

	if (!(pml4_get_page (t->pml4, (uint64_t)page->va & ~PGMASK) == NULL
			&& pml4_set_page (t->pml4, (uint64_t)page->va & ~PGMASK, frame->kva, writable)))
	{
		palloc_free_page (frame->kva);
		return false;
	}

	frame->kva = (uint64_t)frame->kva | ((uint64_t)page->va & PGMASK);

	return swap_in (page, frame->kva);
}

void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->spt_hash, page_hash_function, page_compare_va, NULL);
}

unsigned
page_hash_function(const struct hash_elem *h_elem, void *aux)
{
	struct page 	*page = hash_entry(h_elem, struct page, hash_elem);
	uint64_t 		temp_va = pg_round_down(page->va);
	return hash_bytes(&temp_va, sizeof page->va);
}

bool
page_compare_va(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	struct page *page_a = hash_entry(a, struct page, hash_elem);
	struct page *page_b = hash_entry(b, struct page, hash_elem);

	return pg_round_down(page_a->va) < pg_round_down(page_b->va);
}

static void 
hash_copy_action (struct hash_elem *e, void *aux)
{
	struct supplemental_page_table 	*copy_spt = (struct supplemental_page_table *)aux;
	struct supplemental_page_table	*origin_spt = (struct supplemental_page_table *)copy_spt->spt_hash.aux;
	struct page 					*origin_page, *copy_page;
	enum   vm_type					vm_type;
	struct file_info				*f_info;
	bool	   						origin_writable;
	bool							temp_bool;

	origin_page = hash_entry(e, struct page, hash_elem);
	origin_writable = (uint64_t)origin_page->va & PTE_W;
	vm_type = origin_page->operations->type;

	if(vm_type != VM_UNINIT)
		vm_alloc_page(vm_type, origin_page->va, origin_writable);
	else
	{
		enum vm_type type = origin_page->uninit.type;
		void *init = origin_page->uninit.init;

		if(origin_page->uninit.aux)
		{
			f_info = (struct file_info *)calloc(sizeof(struct file_info), 1);
			if(!f_info)	return false;

			memcpy(f_info, origin_page->uninit.aux, sizeof(struct file_info));
		}
		vm_alloc_page_with_initializer(page_get_type(origin_page), origin_page->va, origin_writable, init, f_info);
	}

	switch(vm_type)
	{
		case VM_UNINIT:
			break;

		case VM_ANON:
			copy_page = spt_find_page(copy_spt, origin_page->va);
			vm_do_claim_page(copy_page);
			memcpy(copy_page->frame->kva, origin_page->frame->kva, PGSIZE);
			break;

		case VM_FILE:
			copy_page = spt_find_page(copy_spt, origin_page->va);
			vm_do_claim_page(copy_page);
			memcpy(copy_page->frame->kva, origin_page->frame->kva, PGSIZE);
			break;
	}
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst, struct supplemental_page_table *src) 
{
	enum   vm_type					vm_type;
	struct file_info				*f_info;
	bool	   						parent_writable;
	
	dst->spt_hash.aux = src;
	src->spt_hash.aux = dst;

	hash_apply(&src->spt_hash, hash_copy_action);

	return (hash_size(&src->spt_hash) == hash_size(&dst->spt_hash));
}

static void
hash_destroy_action(struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);

	vm_dealloc_page(page);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	if(!hash_empty(&spt->spt_hash))
		hash_clear(&spt->spt_hash, hash_destroy_action);
}

struct page *
page_lookup (const void *va, struct supplemental_page_table *spt) {
  struct page p;
  struct hash_elem *e;

  p.va = va;
  e = hash_find (&spt->spt_hash, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

// ========================================================== Debug Tool ====================================================================================
void 
print_spt(void) {
  struct hash *h = &thread_current()->spt.spt_hash;
  struct hash_iterator i;

  printf("============= {%s} SUP. PAGE TABLE (%d entries) =============\n", thread_current()->name, hash_size(h));
  printf("   USER VA    | KERN VA (PA) |     TYPE     | STK | WRT | DRT(K/U) \n");

  void *va, *kva;
  enum vm_type type;
  char *type_str, *stack_str, *writable_str, *dirty_k_str, *dirty_u_str;
  stack_str = " - ";

  hash_first (&i, h);
  struct page *page;
  // uint64_t *pte;
  while (hash_next (&i)) {
    page = hash_entry (hash_cur (&i), struct page, hash_elem);

    va = page->va;
    if (page->frame) {
      kva = page->frame->kva;
      // pte = pml4e_walk(thread_current()->pml4, page->va, 0);
      writable_str = (uint64_t)page->va & PTE_W ? "YES" : "NO";
      // dirty_str = pml4_is_dirty(thread_current()->pml4, page->va) ? "YES" : "NO";
      // dirty_k_str = is_dirty(page->frame->kpte) ? "YES" : "NO";
      // dirty_u_str = is_dirty(page->frame->upte) ? "YES" : "NO";
    }
    else {
      kva = NULL;
      dirty_k_str = " - ";
      dirty_u_str = " - ";
    }
    type = page->operations->type;
    if (VM_TYPE(type) == VM_UNINIT) {
      type = page->uninit.type;
      switch (VM_TYPE(type)) {
        case VM_ANON:
          type_str = "UNINIT-ANON";
          break;
        case VM_FILE:
          type_str = "UNINIT-FILE";
          break;
        case VM_PAGE_CACHE:
          type_str = "UNINIT-P.C.";
          break;
        default:
          type_str = "UNKNOWN (#)";
          type_str[9] = VM_TYPE(type) + 48; // 0~7 사이 숫자의 아스키 코드
      }
      // stack_str = type & IS) ? "YES" : "NO";
      struct file_page_args *fpargs = (struct file_page_args *)page->uninit.aux;
      writable_str = (uint64_t)page->va & PTE_W ? "(Y)" : "(N)";
    }
    else {
      stack_str = "NO";
      switch (VM_TYPE(type)) {
        case VM_ANON:
          type_str = "ANON";
          // stack_str = page->anon.is_stack ? "YES" : "NO";
          break;
        case VM_FILE:
          type_str = "FILE";
          break;
        case VM_PAGE_CACHE:
          type_str = "PAGE CACHE";
          break;
        default:
          type_str = "UNKNOWN (#)";
          type_str[9] = VM_TYPE(type) + 48; // 0~7 사이 숫자의 아스키 코드
      }


    }
    printf(" %12p | %12p | %12s | %3s | %3s |  %3s/%3s \n",
      pg_round_down (va), kva, type_str, stack_str, writable_str, dirty_k_str, dirty_u_str);
  }
}