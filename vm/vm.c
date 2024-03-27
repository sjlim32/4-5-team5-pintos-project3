/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/mmu.h"
#include "threads/vaddr.h"
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

static uint64_t get_offset(void *);
static void set_offset(int *, int *);
static void hash_copy_action (struct hash_elem *, void *);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	// printf("#########################%s#######################\n", "vm_alloc_page_with_initializer");
	struct supplemental_page_table 	*spt = &thread_current ()->spt;
	struct page						*page = malloc(sizeof(struct page));
	void 							*initializer;
	bool							succ = false;

	type = (enum vm_type)VM_TYPE(type);

	if(!page)	goto err;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
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

		/* TODO: Insert the page into the spt. */
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
	// struct hash_elem 	*temp_elem;
	// struct hash			temp_hash = spt->spt_hash;
	/* TODO: Fill this function. */
	// uint64_t key = hash_bytes(va, sizeof va);

	// printf("#########################%s#######################\n", "spt_find_page");

	if(hash_empty(&spt->spt_hash))
		return page;

	// temp_elem = &temp_hash.buckets[key];
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

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
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
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table 	*spt = &thread_current ()->spt;
	struct page 					*page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	if(is_kernel_vaddr(addr))
		return false;

	// printf("thread_current()->name: %s\n", thread_current()->name);

	// printf("#########################%s#######################\n", "vm_try_handle_fault");

	page = spt_find_page(spt, addr);

	if(!page)
		return false;
	// else
	// 	printf("page isn't NULL\n");

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
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	// page = (struct page *)calloc(sizeof(struct page), 1);
	// if(!page) return false;

	// page->va = va;

	// printf("#########################%s#######################\n", "vm_claim_page");

	page = spt_find_page(&thread_current()->spt, va);

	// print_spt();

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

	/* Set links */
	frame->page = page;
	page->frame = frame;

	// printf("#########################%s#######################\n", "vm_do_claim_page");

	// if(frame->kva)     // anonymous or file_backed 구분
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// pml4_set_page(&thread_current()->pml4, (uint64_t)page->va & ~PGMASK, frame->kva, true);

	// page->va = (uint64_t)page->va & PTE_W;
	// set_offset(frame->kva, page->va);


	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */

	writable = (uint64_t)page->va & PTE_W;

	if (!(pml4_get_page (t->pml4, (uint64_t)page->va & ~PGMASK) == NULL
			&& pml4_set_page (t->pml4, (uint64_t)page->va & ~PGMASK, frame->kva, writable))) {
		// printf("fail\n");
		palloc_free_page (frame->kva);
		return false;
	}

	frame->kva = (uint64_t)frame->kva | ((uint64_t)page->va & PGMASK);

	return swap_in (page, frame->kva);
}

static void
set_offset(int *kva, int *va)
{
	kva = (uint64_t)kva | get_offset(va);
}

static uint64_t
get_offset(void *va)
{
	return (uint64_t)va & 0xfff;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	// printf("#########################%s#######################\n", "supplemental_page_table_init");
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
	bool	   						origin_writable;
	bool							temp_bool;

	// printf("#########################%s#######################\n", "hash_copy_action");

	origin_page = hash_entry(e, struct page, hash_elem);

	vm_type = page_get_type(origin_page);
	origin_writable = (uint64_t)origin_page->va & PTE_W;

	if(vm_type != VM_UNINIT)
		vm_alloc_page(vm_type, origin_page->va, origin_writable);

	copy_page = spt_find_page(copy_spt, origin_page->va);

	switch(vm_type)
	{
		case VM_UNINIT:
			// copy_page->uninit.aux = origin_page->uninit.aux;
			// copy_page->uninit.init = origin_page->anon.init;
			// copy_page->uninit.page_initializer = origin_page->uninit.page_initializer;
			break;

		case VM_ANON:
			// copy_page->anon.aux = origin_page->anon.aux;
			// copy_page->anon.init = origin_page->anon.init;
			// copy_page->anon.page_initializer = origin_page->anon.page_initializer;
			vm_do_claim_page(copy_page);
			// printf("exit_status: %d\n", thread_current()->exit_status);
			memcpy(copy_page->frame->kva, origin_page->frame->kva, PGSIZE);
			break;

		case VM_FILE:
			// copy_page->file.aux = origin_page->file.aux;
			// copy_page->file.init = origin_page->file.init;
			// copy_page->file.page_initializer = origin_page->file.page_initializer;
			vm_do_claim_page(copy_page);
			memcpy(copy_page->frame->kva, origin_page->frame->kva, PGSIZE);
			break;
	}

	// hash_insert(&copy_spt->spt_hash, &copy_page->hash_elem);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	
	// printf("#########################%s#######################\n", "supplemental_page_table_copy");
	// dst->spt_hash.hash = src->spt_hash.hash;
	// dst->spt_hash.less = src->spt_hash.less;
	// dst->spt_hash.aux = src;

	src->spt_hash.aux = dst;
	hash_apply(&src->spt_hash, hash_copy_action);

	return hash_size(&src->spt_hash) == hash_size(&dst->spt_hash);
}

static void
hash_destroy_action(struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	// if(page->frame)
	// {
 	// 	free(page->frame);
	// 	palloc_free_page(page->frame->kva);
	// }
	vm_dealloc_page(page);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	if(!hash_empty(&spt->spt_hash))
	{
		hash_clear(&spt->spt_hash, hash_destroy_action);
		// hash_destroy(&spt->spt_hash, hash_destroy_action);
	}
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