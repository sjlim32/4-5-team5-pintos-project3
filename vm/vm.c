/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
#include "threads/pte.h"
#include "threads/mmu.h"

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

/* 주어진 유형으로 초기화되지 않은 페이지를 생성합니다.
 * uninit 페이지의 swap_in 핸들러는 페이지를 자동으로 유형에 따라 초기화하고 주어진 AUX로 INIT를 호출합니다.
 * 페이지 구조를 가져온 후 페이지를 프로세스의 보충 페이지 테이블에 삽입합니다.
 * vm.h에 정의된 VM_TYPE 매크로를 사용하는 것이 편리할 수 있습니다. */
/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	type = (enum vm_type)VM_TYPE (type);

	struct page* n_page = malloc (sizeof (struct page));

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
		switch (type)
		{
		case VM_ANON:
			uninit_new (n_page, upage, init, type, aux, anon_initializer);
			break;

		case VM_FILE:
			uninit_new (n_page, upage, init, type, aux, file_backed_initializer);
			printf ("fail! type is VM_FILE now\n");
			break;

		case VM_PAGE_CACHE:
			printf ("fail! type is VM_PAGE_CACHE now\n");
			break;
		
		default:
			break;
		}
		spt_insert_page (spt, n_page);
	}
err:
	return false;
}


/* 주어진 보조 페이지 테이블에서 va에 해당하는 struct page를 찾습니다. 실패하면 NULL을 반환합니다. */
/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *page = page_lookup (&spt->spt_hash, va);
	return page;
}

/* struct 페이지를 주어진 보조 페이지 테이블에 삽입합니다.
 * 이 함수는 주어진 보조 페이지 테이블에 가상 주소가 존재하지 않는지 확인해야 합니다. */
/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	int succ = hash_insert (&spt->spt_hash, &page->h_elem) == NULL ? true : false;
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

/* palloc_get_page를 호출하여 사용자 풀에서 새로운 물리 페이지를 가져옵니다.
 * 사용자 풀에서 페이지를 성공적으로 가져오면 프레임을 할당하고 해당 멤버를 초기화한 후 반환합니다.
 * vm_get_frame을 구현한 후에는 페이지 할당 실패 시에 대비해 지금은 페이지 할당 실패의 경우에 대해
 * PANIC("todo")로 표시하면 됩니다.
 * 지금은 스왑아웃을 처리하지 않아도 됩니다. */
/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame *)calloc (sizeof(struct frame), 1);
	if (frame == NULL) {
		// 맛이 간거임.
		return NULL;
	}

	/* TODO: Fill this function. */
	if ((frame->kva = (struct page *)palloc_get_page (PAL_ZERO | PAL_USER)) == NULL) {
		// 스왑 디스크 해줘야 함.
		PANIC("todo: swap_disk\n");
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* 주소가 더 이상 폴트된 주소가 아니도록 하기 위해 스택 크기를 증가시킵니다.
 * 할당을 처리할 때 addr를 PGSIZE로 내림합니다.

 * 대부분의 운영 체제는 스택 크기에 대한 절대적인 제한을 부과합니다.
 * 일부 운영 체제에서는 ulimit 명령을 사용하여 제한을 사용자가 조정할 수 있습니다.
 * 이 프로젝트에서는 최대 스택 크기를 1MB로 제한해야 합니다. */
/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* 이 함수는 페이지 폴트 예외를 처리하는 동안 userprog/exception.c의 page_fault에서 호출됩니다.
 * 이 함수에서는 페이지 폴트가 스택 성장에 대한 유효한 경우인지 확인해야 합니다.
 * 폴트를 스택 성장으로 처리할 수 있다고 확인되면 폴트된 주소와 함께 vm_stack_growth를 호출합니다. */
/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	// if (addr > KERN_BASE && ) {
	// 	return false;
	// }

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* va를 할당하려면 페이지를 가져와야 합니다. 먼저 페이지를 가져와서 그 페이지를 호출합니다. */
/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	// 용의자 후보 1
	struct page *page = (struct page*)calloc (sizeof(struct page), 1);

	if (page == NULL) {
		return false;
	}

	/* TODO: Fill this function */
	page->va = va;
	page->uninit.type = VM_UNINIT;

	return vm_do_claim_page (page);
}

/* 페이지를 요청하면 물리 프레임을 할당합니다.
 * 먼저 vm_get_frame을 호출하여 프레임을 가져옵니다(이미 템플릿에서 수행됨).
 * 그런 다음 MMU를 설정해야 합니다. 다시 말해 가상 주소에서 물리 주소로의 매핑을 페이지 테이블에 추가합니다.
 * 반환 값은 작업이 성공했는지 여부를 나타내야 합니다. */
/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	if (frame == NULL) {
		return false;
	}

	/* Set links */
	frame->page = page;
	page->frame = frame;
	frame->kva = (uint64_t)frame->kva | ((uint64_t)page->va & 0xfff);

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *t = thread_current ();
	bool writable = (uint64_t)page->va & PTE_W;

	if (pml4_get_page (t->pml4, page->va) == NULL
			&& pml4_set_page (t->pml4, page->va, frame->kva, writable)) {
		printf("fail\n");
		palloc_free_page (frame->kva);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* 보조 페이지 테이블을 초기화합니다.
 * 보조 페이지 테이블에 사용할 데이터 구조를 선택할 수 있습니다.
 * 이 함수는 새 프로세스가 시작될 때(initd의 userprog/process.c에서)
 * 또는 프로세스가 복제될 때(__do_fork의 userprog/process.c에서) 호출됩니다. */
/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	spt->physical_frame = NULL;
	spt->type = VM_UNINIT;

	hash_init (&spt->spt_hash, page_hash, page_less, NULL);
}

/* 부모 프로세스(즉, fork())의 실행 컨텍스트를 상속해야 할 때 사용하는 보충 페이지 테이블을 src에서 dst로 복사합니다.
 * src의 보충 페이지 테이블의 각 페이지를 반복하고 dst의 보충 페이지 테이블에 해당 페이지의 항목을 정확히 복사합니다.
 * 즉시 요청 페이지를 할당하고 그 즉시 그것을 요청해야합니다. */
/* Copy supplemental page table from src to dst */
void hash_table_copy (struct hash_elem *e, void *aux) {
	struct supplemental_page_table *dst = aux;
	hash_insert (&dst->spt_hash, e);
}

bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	src->spt_hash.aux = dst;
	hash_apply (&src->spt_hash, hash_table_copy);

	// TODO
}

/* 보충 페이지 테이블에서 보유한 모든 리소스를 해제합니다.
 * 이 함수는 프로세스가 종료될 때 호출됩니다(userprog/process.c의 process_exit()).
 * 페이지 테이블의 각 페이지 항목을 반복하고 테이블의 페이지에 대해 destroy(page)를 호출합니다.
 * 이 함수에서는 실제 페이지 테이블(pml4)과 물리 메모리(palloc-ed 메모리)를 걱정할 필요가 없습니다.
 * 호출자가 보충 페이지 테이블이 정리된 후에 정리합니다. */
/* Free the resource hold by the supplemental page table */
void hash_table_kill (struct hash_elem *e, void *aux) {
	struct page* d_page = hash_entry (e, struct page, h_elem);

	palloc_free_page (d_page->frame->kva);
	free (d_page->frame);
	vm_dealloc_page (d_page);
}

void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy (&spt->spt_hash, hash_table_kill);
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, h_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, h_elem);
  const struct page *b = hash_entry (b_, struct page, h_elem);

  return a->va < b->va;
}

/* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
struct page *
page_lookup (const struct hash* pt_hash, const void *address) {
  struct page p;
  struct hash_elem *e;

  p.va = address;
  e = hash_find (pt_hash, &p.h_elem);
  return e != NULL ? hash_entry (e, struct page, h_elem) : NULL;
}