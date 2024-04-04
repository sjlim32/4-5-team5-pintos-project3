/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "kernel/hash.h"
#include "threads/init.h"

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
	return true;
}

/* 파일에서 내용을 읽어 메모리로 스왑인합니다. 파일 시스템과 동기화해야 합니다. */
/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	// printf ("=========file_backed_swap_in start==============\n"); ////////////////////////////////
	struct file_page *file_page = &page->file;

	vm_initializer *init = page->uninit.init;
	void *aux = page->uninit.aux;
	// page->va = (uint64_t)page->va | PTE_P;

	/* TODO: You may need to fix this function. */
	return file_backed_initializer (page, page->operations->type, kva) &&
		lazy_load_segment (page, aux);
}

/* 먼저 구현 */
/* 페이지의 내용을 파일로 다시 기록하여 페이지를 스왑 아웃합니다.
 * 페이지가 변경되었는지 여부를 먼저 확인하는 것이 좋습니다.
 * 변경되지 않았으면 파일의 내용을 수정할 필요가 없습니다.
 * 페이지를 스왑 아웃한 후 페이지의 더티 비트를 끄는 것을 잊지 마세요. */
/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	// printf ("=========file_backed_swap_out start==============\n"); ////////////////////////////////
	uint64_t current_pml4 = thread_current ()->pml4;
	struct file_page *file_page UNUSED = &page->file;
	file_info *f_info = page->uninit.aux;

	if (f_info != NULL) {
		if (pml4_is_dirty (current_pml4, page->va) || pml4_is_dirty (base_pml4, page->frame->kva)) {
			file_write_at (f_info->file, pg_round_down (page->frame->kva), f_info->read_bytes, f_info->ofs);
		}
		// free (f_info);
	}
	pml4_set_dirty (current_pml4, page->va, 0);
	pml4_set_dirty (base_pml4, page->frame->kva, 0);

	// Hardware does not re-zero the accessed bit.
	// therefore, we turn off the PTE_A by direct.
	// pml4_set_accessed (current_pml4, page->va, 0);
	// pml4_set_accessed (base_pml4, page->frame->kva, 0);

	pml4_clear_page (thread_current ()->pml4, pg_round_down (page->va));
	// page->va = (uint64_t)page->va & ~PTE_P;

	return true;
}

/* 연관된 파일을 닫아 파일 백드 페이지를 소멸시킵니다.
 * 내용이 더러운 경우 변경 사항을 파일에 다시 기록하세요.
 * 이 함수에서 페이지 구조체를 명시적으로 해제할 필요는 없습니다.
 * file_backed_destroy를 호출하는 호출자가 처리합니다.
 */
/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	// printf ("=========start file_backed_destroy %p, %p\n", page, page->va); ////////////////////////////////
	struct file_page *file_page UNUSED = &page->file;
	file_info *f_info = page->uninit.aux;

	if (f_info != NULL) {
		if (pml4_is_dirty (thread_current ()->pml4, page->va) || pml4_is_dirty (base_pml4, page->frame->kva)) {
			file_write_at (f_info->file, pg_round_down (page->frame->kva), file_length (f_info->file), f_info->ofs);
		}
		free (f_info);
	}
	if (page->frame) {
		list_remove (&page->frame->evict_elem);
		free (page->frame);
 	}

	pml4_clear_page (thread_current ()->pml4, pg_round_down (page->va));
}

bool
mmap_lazy_load_segment (struct page *page, void *aux) {
	// printf ("=========start mmap_lazy_load_segment==============\n"); ////////////////////////////////
	file_info *f_info = aux;
	void* kaddr = pg_round_down (page->frame->kva);

	// print_spt ();
	file_seek (f_info->file, f_info->ofs);
	// printf ("=========mmap_lazy_load_segment check_addr %p, %p==============\n", page->va, kaddr); ////////////////////////////////
	if (file_read (f_info->file, kaddr, f_info->read_bytes) > (int) f_info->read_bytes) {
		palloc_free_page (kaddr);
		return false;
	}

	return true;
}

/* fd로 열린 파일에서 오프셋 바이트부터 시작하여 프로세스의 가상 주소 공간 addr에 length 바이트를 매핑합니다.
 * 파일 전체가 addr에서 시작하는 연속적인 가상 페이지로 매핑됩니다.
 * 파일의 길이가 PGSIZE의 배수가 아닌 경우 최종 매핑된 페이지의 일부 바이트가 파일의 끝을 넘어서게 됩니다.
 * 페이지가 로드될 때 이러한 바이트를 0으로 설정하고, 페이지가 디스크로 다시 기록될 때 이러한 바이트를 삭제하세요.
 * 성공하면 파일이 매핑된 가상 주소를 반환합니다.
 * 실패하면 파일을 매핑할 수 없는 유효하지 않은 주소인 NULL을 반환해야 합니다.
 *
 * mmap을 호출하면 파일이 0바이트 길이일 경우 실패합니다.
 * addr이 페이지에 맞지 않는 경우 또는 이미 매핑된 페이지 집합과 겹치는 경우
 * (스택 또는 실행 가능한 로드 시간에 매핑된 페이지 포함) 실패해야 합니다.
 * Linux에서 addr이 NULL인 경우 커널은 매핑을 생성할 적절한 주소를 찾습니다.
 * 단순성을 위해 주어진 addr에 mmap을 시도하세요. 따라서 addr이 0인 경우 실패해야 합니다.
 * 이는 일부 Pintos 코드가 가상 페이지 0이 매핑되지 않았다고 가정하기 때문입니다.
 * 길이가 0인 경우 mmap은 실패해야 합니다.
 * 마지막으로, 콘솔 입력 및 출력을 나타내는 파일 디스크립터는 매핑할 수 없습니다.
 * 
 * 메모리 맵 페이지는 익명 페이지와 마찬가지로 게으르게 할당되어야 합니다.
 * 페이지 개체를 만들기 위해 vm_alloc_page_with_initializer 또는 vm_alloc_page를 사용할 수 있습니다. */
/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	// printf  ("==========start do_mmap: %p\n", addr); //////////////////////////////////////////
	void *init_addr = addr;
	size_t init_length = length;
	size_t read_bytes = file_length (file);
	
	// for debug.
	static int i = 0;

	while ((int)length > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		file_info* f_info;

		// TODO: destroy에서 free
		if ((f_info = malloc (sizeof (file_info))) == NULL) {
			printf ("[do_mmap] FAILED: there is no heap area to malloc\n");
			return NULL;
		}
		
		f_info->file = file_reopen (file);
		f_info->read_bytes = page_read_bytes;
		f_info->ofs = offset;
		f_info->remain_bytes = read_bytes;
		// printf ("in do_mmap: %p\n\n", f_info->file);

		// printf ("in do_mmap %d\n", page_read_bytes);
		// printf ("in do_mmap %d\n", ++i);
		if (!vm_alloc_page_with_initializer (VM_FILE, addr,
					writable, mmap_lazy_load_segment, f_info)) {
			printf ("[do_mmap] FAILED: alloc page initializing failed.\n");
			return NULL;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
		length -= PGSIZE;
	}

	struct page *page = spt_find_page (&thread_current ()->spt, init_addr);
	page->file_length = init_length;

	return init_addr;
}

/* 아직 매핑되지 않은 동일한 프로세스에 의해 이전에 호출된 mmap에 의해 반환된 가상 주소 범위 addr에 대한 매핑을 해제합니다.

 * 모든 매핑은 프로세스가 종료될 때 묵시적으로 해제됩니다.
 * 매핑이 해제되면 명시적 또는 묵시적으로 프로세스에 의해 기록된 모든 페이지가 파일에 기록되어야 하며,
 * 기록되지 않은 페이지는 그렇지 않아야 합니다. 그런 다음 페이지가 프로세스의 가상 페이지 목록에서 제거됩니다.

 * 파일을 닫거나 제거하면 해당 파일의 매핑이 해제되지 않습니다.
 * 한 번 생성된 매핑은 munmap이 호출되거나 프로세스가 종료될 때까지 유효합니다.
 * 유닉스 규칙을 따릅니다. 자세한 내용은 열린 파일 제거를 참조하십시오.
 * 각 매핑에 대해 파일을 별도로 참조하고 독립적으로 얻으려면 file_reopen 함수를 사용해야 합니다.

 * 동일한 파일을 두 개 이상의 프로세스가 매핑하는 경우 일관된 데이터를 볼 필요가 없습니다.
 * 유닉스는 두 매핑이 동일한 물리적 페이지를 공유하도록 만들고 mmap 시스템 호출에는
 * 페이지를 공유 또는 개인(쓰기 시 복사)로 지정할 수 있는 인수도 있습니다.

 * vm_file_init 및 vm_file_initializer를 vm/vm.c에서 필요에 따라 수정할 수 있습니다. */
/* Do the munmap */
void
do_munmap (void *addr) {
	struct page *page = spt_find_page (&thread_current ()->spt, addr);
	// printf ("================start do_munmap %p, %p\n", page, addr); ////////////////////////////////
	size_t length = page->file_length;
	file_info *f_info = page->uninit.aux;
	
	// for debug.
	static int i = 0;

	while (page != NULL && length > 0) {
		
		hash_delete (&thread_current ()->spt.spt_hash, &page->h_elem);
		vm_dealloc_page (page);

		length -= PGSIZE;
		addr += PGSIZE;

		page = spt_find_page (&thread_current ()->spt, addr);
	}
}
