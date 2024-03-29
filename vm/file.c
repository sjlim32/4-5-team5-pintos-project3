/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "threads/malloc.h"
#include "include/userprog/process.h"

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
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
}

static bool
lazy_load(struct page *page, void *aux)
{
	struct file_info 	*f_info = (struct file_info *)aux;
	struct file 		*file = f_info->file;
	size_t				read_bytes = f_info->page_read_bytes;
	off_t				offset = f_info->off;

	uint8_t *kpage = (uint8_t *)page->frame->kva;

	printf("page->frame->kva: %x\n", page->frame->kva);
	file_seek(file, offset);
	if (file_read (file, kpage, read_bytes) != (int) read_bytes) 
	{
		palloc_free_page (kpage);
		return false;
	}

	// page->frame->kva가 항상 같은 곳을 가리킴
	// 왜...??

	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset)
{
	size_t total_length = length;

	// printf("addr: %x, length: %d, offset: %d, writable: %d\n", addr, length, offset, writable);

	file_seek (file, offset);
	while (total_length > 0) 
	{
		// printf("111111111\n");
		size_t read_bytes = total_length < PGSIZE ? total_length : PGSIZE;
		struct file_info *f_info = (struct file_info *)malloc(sizeof(struct file_info));
		if(!f_info) return;

		// printf("22222222222\n");
		f_info->file = file;
		f_info->page_read_bytes = read_bytes;
		f_info->off = offset;

		void *aux = f_info;

		if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load, aux))
		{
			free(f_info);
			return false;
		}

		// printf("3333333333333\n");
		
		print_spt();

		// struct page *temp_page;
		// if((temp_page = spt_find_page(&thread_current()->spt, addr)) != NULL)
		// {
		// 	// printf("spt_find_page is NULL\n");
		// 	printf("temp_page->va: %s\n", temp_page->va);
		// }


		if(!vm_claim_page(addr))
		{
			// printf("vm_claim_page failed\n");
			return false;
		}

		// printf("44444444444444\n");
		total_length -= read_bytes;
		offset += read_bytes;
		addr += PGSIZE;
	}

	// printf("5555555555555555\n");
}

/* Do the munmap */
void
do_munmap (void *addr) {

}
