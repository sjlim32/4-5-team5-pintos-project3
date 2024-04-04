/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "kernel/bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

#define ONE_MB (1 << 20) // 1MB
static struct bitmap *b_map;

/* 익명 페이지의 스왑을 지원하기 위해 스왑 디스크라는 임시 백드 스토리지를 제공합니다.
 * 익명 페이지의 스왑에 스왑 디스크를 활용할 것입니다.
 * 이 함수에서 스왑 디스크를 설정해야 합니다.
 * 또한 스왑 디스크의 빈 영역과 사용 중인 영역을 관리하기 위한 데이터 구조가 필요합니다.
 * 스왑 영역은 PGSIZE(4096 바이트)의 단위로 관리됩니다.*/
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get (1, 1);
	size_t swap_size = disk_size (swap_disk) / PAGE_DISK_SEGMENT;
	// printf ("in anon_init ================ { %d }\n", swap_size);
	b_map = bitmap_create (swap_size);
}

/* 이것은 익명 페이지의 이니셜라이저입니다.
 * 익명 페이지의 스왑을 지원하기 위해 anon_page에 몇 가지 정보를 추가해야 합니다. */
/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	// printf ("=========anon_initializer start==============\n"); ////////////////////////////////
	/* Set up the handler */
	page->operations = &anon_ops;

	// TODO: disk 관련
	struct anon_page *anon_page = &page->anon;
	return true;
}

/* 디스크에서 데이터 내용을 읽어 메모리로 스왑인합니다.
* 데이터의 위치는 페이지가 스왑 아웃될 때 페이지 구조체에 저장되어 있어야 합니다.
* 스왑 테이블을 업데이트하는 것을 잊지 마세요(스왑 테이블 관리 참조). */
/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	printf ("=========anon_swap_in start==============\n"); ////////////////////////////////
	struct anon_page *anon_page = &page->anon;
	size_t swap_slot = anon_page->swap_slot;
	void* kaddr = pg_round_down (page->frame->kva);

	for (int i = 0 ; i < PAGE_DISK_SEGMENT; i++) {
		disk_read (swap_disk, swap_slot * PAGE_DISK_SEGMENT + i, kaddr + (i * DISK_SECTOR_SIZE));
	}
	page->va = (uint64_t)page->va | PTE_P;

	bitmap_set (b_map, swap_slot, 0);
	anon_page->swap_slot = -1;

	return true;
}

/* swap_in 보다 먼저 구현
 * 메모리의 내용을 디스크로 복사하여 익명 페이지를 스왑 아웃합니다.
 * 먼저 디스크에서 빈 스왑 슬롯을 찾아야 하고 페이지 데이터를 해당 슬롯으로 복사해야 합니다.
 * 데이터의 위치는 페이지 구조체에 저장되어야 합니다.
 * 디스크에 더 이상 빈 슬롯이 없는 경우 커널을 패닉 상태로 전환할 수 있습니다. */
/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	// printf ("=========anon_swap_out start==============\n"); ////////////////////////////////
	// printf ("page va %p\n", page->va);
	// printf ("page frame kva %p\n", page->frame->kva);
	// print_spt ();
	struct anon_page *anon_page = &page->anon;
	size_t swap_slot = bitmap_scan_and_flip (b_map, 0, 1, 0);

	if (swap_slot == BITMAP_ERROR) {
		PANIC ("[anon_swap_out] FAILED: bitmap space not enough\n");
	}

	void* kaddr = pg_round_down (page->frame->kva);
	anon_page->swap_slot = swap_slot;

	for (int i = 0; i < PAGE_DISK_SEGMENT; i++) {
		disk_write (swap_disk, swap_slot * PAGE_DISK_SEGMENT + i, kaddr + (i * DISK_SECTOR_SIZE));
	}

	pml4_clear_page (thread_current ()->pml4, pg_round_down (page->va));
	// page->va = (uint64_t)page->va & ~PTE_P;

	return true;
}

/* 익명 페이지가 보유한 리소스를 해제합니다.
 * 페이지 구조를 명시적으로 해제할 필요가 없습니다. 호출자가 해야합니다. */
/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	
	if (page->uninit.aux != NULL) {
		free (page->uninit.aux);
	}
	
	if (page->frame) {
    list_remove (&page->frame->evict_elem);
    free (page->frame);
  	}
}
