/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/syscall.h"

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
/* 파일 백드 페이지 하위 시스템을 초기화합니다. 이 함수에서 파일 백드 페이지와 관련된 모든 것을 설정할 수 있습니다. */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
/* 파일 백드 페이지를 초기화합니다.
 * 함수는 먼저 페이지->operations에서 파일 백드 페이지의 핸들러를 설정합니다.
 * 페이지 구조체에 파일 백드 메모리를 백드하는 파일과 같은 몇 가지 정보를 업데이트할 수 있습니다.
*/
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* 파일에서 내용을 읽어 메모리로 스왑인합니다. 파일 시스템과 동기화해야 합니다. */
/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 먼저 구현 */
/* 페이지의 내용을 파일로 다시 기록하여 페이지를 스왑 아웃합니다.
 * 페이지가 변경되었는지 여부를 먼저 확인하는 것이 좋습니다.
 * 변경되지 않았으면 파일의 내용을 수정할 필요가 없습니다.
 * 페이지를 스왑 아웃한 후 페이지의 더티 비트를 끄는 것을 잊지 마세요. */
/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 연관된 파일을 닫아 파일 백드 페이지를 소멸시킵니다.
 * 내용이 더러운 경우 변경 사항을 파일에 다시 기록하세요.
 * 이 함수에서 페이지 구조체를 명시적으로 해제할 필요는 없습니다.
 * file_backed_destroy를 호출하는 호출자가 처리합니다.
 */
/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
