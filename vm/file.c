/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
/* ------ Project 3 ------ */
#include <stdio.h>
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/file.h"
#include <round.h>

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
  struct file_page *file_page = page;
  if (page->uninit.aux) {
    free (page->uninit.aux);
  }
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
  /* MMAP
fd로 열린 파일의 오프셋 바이트부터, 길이만큼을 프로스세의 가상주소공간의 addr에 매핑함
전체 파일은 연속된 가상페이지에 매핑돰
파일 길이가 PGSIZE의 배수가 아닌 경우, 최종 매핑 페이지의 일부 바이트가 파일 끝을 넘어 stick out 됨
page_fault 발생 시, 이 바이트를 0으로 설정하고 페이지를 디스크에 다시 쓸 때 버림
? fd의 파일을 가져옴 -> offset + length 만큼 addr에 매핑함
? fd의 파일을 매핑 시, 연속된 가상페이지 주소에 할당됨 ( lazy load 필요, vm_alloc_page_with_initializer 활용)
? file의 length 가 PGSIZE 의 배수가 아닌 경우, 최종 매핑된 페이지의 일부 바이트가 넘어갈 수 있음
? page_fault 가 발생하면, 넘어간 일부 바이트를 0으로 설정하고 페이지를 디스크에 다시 쓸 때 버림

? 성공 시, 파일이 매핑된 가상 주소를 리턴
? 실패 시, NULL 리턴 ( 실패하는 경우 -> addr = 0 , length = 0, fd가 stdin & stdout)
*/

// printf ("file ( do_mmap ) : addr { %p } , length { %d }, w { %d }, f { %p }, offs { %d }\n",
  // addr, (int)length, writable, file, offset);

  // uint32_t zero_length = ROUND_UP (offset + (int)length, PGSIZE);
  void *init_addr = addr;
  size_t init_length = length;
  uint32_t read_bytes = file_length (file);

  // while (length > 0 || zero_length > 0) {
  // while (read_bytes > 0) {
  while (length > 0) {

    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    // size_t page_zero_length = PGSIZE - page_length;
    file_info *f_info;

    if (!(f_info = malloc (sizeof (file_info)))) {
      return NULL;
    }
    f_info->file = file;
    f_info->read_bytes = page_read_bytes;
    f_info->ofs = offset;

    if (!vm_alloc_page_with_initializer (VM_FILE | IS_STACK, addr, writable, lazy_load_segment, f_info)) {
      // printf ("file ( do_mmap ) : FAILED - vm_alloc_page_with_initializer\n");
      return NULL;
    }

    // printf ("file ( do_mmap ) : FINISH - vm_clame_page\n");
    /* Advance. */
    read_bytes -= page_read_bytes;
    // zero_length -= page_zero_length;
    addr += PGSIZE;
    offset += page_read_bytes;
    length -= PGSIZE;
  }
  struct page *page = spt_find_page (&thread_current ()->spt, init_addr);
  page->file_length = init_length;

  return init_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
  struct thread *curr = thread_current ();
  struct page *page = spt_find_page (&curr->spt, addr);
  uint32_t rep = (page->file_length % PGSIZE) == 0 ? page->file_length / PGSIZE : (page->file_length / PGSIZE) + 1;
  // print_spt ();
  // printf ("do_munmap : addr = { %p, %d }\n", addr, page->file_length);
  // printf ("do_munmap : rep, file_length = { %d }\n", rep);

  while (rep) {
    printf ("do_munmap : rep START { %d }\n", rep);
    rep--;
    // printf (" do_mummap : page = { %p }\n", page);
    hash_delete (&curr->spt.spt_hash, &page->h_elem);
    // printf ("do_munmap : hash_delete\n");
    vm_dealloc_page (page);
    // printf ("do_munmap : dealloc_page\n");

    if (rep) {
      // printf (" addr = { %p }\n", addr);
      page = spt_find_page (&curr->spt, addr + PGSIZE);
    }
  }
  // print_spt ();
}