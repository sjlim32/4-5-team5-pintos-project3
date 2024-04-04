/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "threads/malloc.h"
#include "include/userprog/process.h"
#include "include/lib/round.h"
#include "threads/mmu.h"
#include "filesys/file.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "lib/string.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

static bool lazy_load(struct page *page, void *aux);

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
	struct file_page *file_page = &page->file;

	page->va = (uint64_t)page->va | PTE_P;
	file_page->aux = page->uninit.aux;

	return file_backed_initializer(page, file_page->type, kva);
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
	struct file_info *f_info;

	printf("start file backed swap out\n");

	if(file_page->aux)
	{
		f_info = file_page->aux;

		if(pml4_is_dirty(thread_current()->pml4, page->va) || pml4_is_dirty(base_pml4, page->frame->kva))
			file_write_at(f_info->file, pg_round_down(page->frame->kva), file_length(f_info->file), f_info->off);

		free(file_page->aux);
	}

	pml4_set_dirty(thread_current()->pml4, page->va, 0);
	pml4_set_dirty(base_pml4, page->frame->kva, 0);

	page->va = (uint64_t)page->va & ~PTE_P;

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page 	*file_page = &page->file;
	struct file_info 	*temp_file = (struct file_info *)page->file.aux;

	if(pml4_is_dirty(thread_current()->pml4, page->va)/*||  pml4_is_dirty(base_pml4, page->frame->kva)*/)
		file_write_at(temp_file->file, pg_round_down(page->frame->kva), temp_file->file_size, temp_file->off);

	if(page->file.aux)
		free(page->file.aux);
}

static bool
lazy_load(struct page *page, void *aux)
{
	struct file_info 	*f_info = (struct file_info *)aux;
	struct file 		*file = f_info->file;
	size_t				read_bytes = f_info->page_read_bytes;
	off_t				offset = f_info->off;

	uint8_t *kpage = (uint8_t *)page->frame->kva;

	memcpy(page->file.aux, f_info, sizeof(struct file_info));

	file_seek(file, offset);
	if (file_read (file, kpage, read_bytes) != (int) read_bytes)
	{
		palloc_free_page (kpage);
		return false;
	}

	return true;
}

void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset)
{
	size_t 		total_length = file_length(file);
	void		*origin_addr = addr;
	size_t		origin_length = length;

	if(length < offset || pg_ofs (addr) != 0 || pg_ofs (offset) != 0)
		return NULL;

	if(!is_user_vaddr(addr))
		return NULL;

	if(offset % PGSIZE != 0 || (int)length <= 0)
		return NULL;

	while ((int)length > 0)
	{
		size_t read_bytes = total_length < PGSIZE ? total_length : PGSIZE;

		struct file_info *f_info = (struct file_info *)malloc(sizeof(struct file_info));
		if(!f_info) return NULL;

		f_info->file = file;
		f_info->page_read_bytes = read_bytes;
		f_info->file_size = file_length(file);
		f_info->off = offset;

		void *aux = f_info;
		if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load, aux))
		{
			free(f_info);
			return NULL;
		}

		total_length -= read_bytes;
		length -= PGSIZE;
		offset += read_bytes;
		addr += PGSIZE;
	}

	struct page *mapping_page = spt_find_page(&thread_current()->spt, origin_addr);
	mapping_page->file_length = origin_length;

	return origin_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) 
{
	struct page 	*target_page = spt_find_page(&thread_current()->spt, addr);
	uint32_t rep = (target_page->file_length % PGSIZE) == 0 ? target_page->file_length / PGSIZE : (target_page->file_length / PGSIZE) + 1;
	size_t 			length = target_page->file_length;

	while(length > 0)
	{
		hash_delete(&thread_current()->spt.spt_hash, &target_page->hash_elem);
		vm_dealloc_page(target_page);

		length -= PGSIZE;

		if(length)
			target_page = spt_find_page(&thread_current()->spt.spt_hash, addr + PGSIZE);
	}
}
