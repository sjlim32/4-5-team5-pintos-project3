/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

static struct bitmap *swap_bitmap;
#define FILE_SECTOR_SIZE ((PGSIZE) / (DISK_SECTOR_SIZE))

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

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);

	// printf("swap_disk size: %d, %d\n", disk_size(swap_disk), FILE_SECTOR_SIZE);

	swap_bitmap = bitmap_create(disk_size(swap_disk) / FILE_SECTOR_SIZE);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;

	// printf("%s\n", "anon_initializer");
	anon_page->sector_num = 0;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	void *kaddr = pg_round_down(page->frame->kva);

	for(int i = 0; i < FILE_SECTOR_SIZE; i++)
		disk_read(swap_disk, (anon_page->sector_num * FILE_SECTOR_SIZE) + i, kaddr + (DISK_SECTOR_SIZE * i));

	bitmap_set(swap_bitmap, anon_page->sector_num, false);
	anon_page->sector_num = 0;
	// page->va = (uint64_t)page->va | PTE_P;


	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	void *kaddr = pg_round_down(page->frame->kva);

	// bitmap_dump(swap_bitmap);

	int swap_slot = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
	if(swap_slot == BITMAP_ERROR)
		PANIC("ANON BITMAP SCAN ERROR");
	
	for(int i = 0; i < FILE_SECTOR_SIZE; i++)
		disk_write(swap_disk, (swap_slot * FILE_SECTOR_SIZE) + i, kaddr + (DISK_SECTOR_SIZE * i));

	// printf("swap_out: %d\n", swap_slot);

	anon_page->sector_num = swap_slot;
	// bitmap_set(swap_bitmap, swap_slot, true);
	// page->va = (uint64_t)page->va & ~PTE_P;
	pml4_clear_page(thread_current()->pml4, pg_round_down(page->va));

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if(page->uninit.aux)
		free(page->uninit.aux);
}
