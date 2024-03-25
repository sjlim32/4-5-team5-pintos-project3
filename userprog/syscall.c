#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
/* ------ Project 2 ------ */
#include <string.h>
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "kernel/stdio.h"
#include "threads/synch.h"
#include "threads/init.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
/* ------------------------ */

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

  lock_init(&filesys_lock);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
//! Project 2 - System calls
void
syscall_handler (struct intr_frame *f UNUSED) {
  //? 시스템콜 호출 번호 - %rax
  //? 인자 - %rdi, $rsi, %rdx, %r10, %r8, %r9

  int sys_number = f->R.rax;

  switch (sys_number) {

    case SYS_HALT:          /* 0 Halt the operating system. */
      halt();
      break;

    case SYS_EXIT:          /* 1 Terminate this process. */
      exit(f->R.rdi);
      break;

    case SYS_FORK:          /* 2 Clone current process. */
      f->R.rax = fork((char *)f->R.rdi, f);
      break;

    case SYS_EXEC:          /* 3 Switch current process. */
      f->R.rax = exec((char *)f->R.rdi);
      break;

    case SYS_WAIT:          /* 4 Wait for a child process to die. */
      f->R.rax = wait(f->R.rdi);
      break;

    case SYS_CREATE:        /* 5 Create a file. */
      f->R.rax = create((char *)f->R.rdi, f->R.rsi);
      break;

    case SYS_REMOVE:        /* 6 Delete a file. */
      f->R.rax = remove((char *)f->R.rdi);
      break;
    case SYS_OPEN:          /* 7 Open a file. */
      f->R.rax = open((char *)f->R.rdi);
      break;

    case SYS_FILESIZE:      /* 8 Obtain a file's size. */
      f->R.rax = filesize(f->R.rdi);
      break;

    case SYS_READ:          /* 9 Read from a file. */
      f->R.rax = read(f->R.rdi, (void *)f->R.rsi, f->R.rdx);
      break;

    case SYS_WRITE:         /* 10 Write to a file. */
      f->R.rax = write(f->R.rdi, (void *)f->R.rsi, f->R.rdx);
      break;

    case SYS_SEEK:          /* 11 Change position in a file. */
      seek(f->R.rdi, f->R.rsi);
      break;

    case SYS_TELL:          /* 12 Report current position in a file. */
      f->R.rax = tell(f->R.rdi);
      break;

    case SYS_CLOSE:         /* 13 Close a file. */
      close(f->R.rsi);
      break;

      case SYS_MMAP:          /* 12 Report current position in a file. */
      f->R.rax = mmap((void *)f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, (off_t)f->R.r8);
      break;

    case SYS_MUNMAP:         /* 13 Close a file. */
      munmap((void *)f->R.rdi);
      break;

    default:
      printf("system call!\n");
      thread_exit ();
  }
}

//! ------------------------ Project 2 : Systemcall ------------------------ *//
static void
halt (void) {
  power_off ();
}

void
exit (int status) {
  struct thread *curr = thread_current ();
  curr->exit_status = status;

  printf ("%s: exit(%d)\n", thread_name(), curr->exit_status);

  thread_exit ();
}

static pid_t
fork (const char *thread_name, struct intr_frame *f) {
  return process_fork(thread_name, f);
}

static int
exec (const char *file) {
  check_addr(file);

  int len = strlen(file) + 1;
  char *file_name = palloc_get_page(PAL_ZERO);
  if (file_name == NULL)
    exit(-1);

  strlcpy(file_name, file, len);

  if (process_exec(file_name) == -1)
    exit(-1);

  palloc_free_page(file_name);

  NOT_REACHED();
}

static int
wait (pid_t pid) {
  return process_wait (pid);
}

static bool
create (const char* file, unsigned initial_size) {
  check_addr(file);
  return filesys_create(file, initial_size);
}

static int
open (const char *file) {
  check_addr(file);
  struct file *f = filesys_open(file);
  if (f == NULL)
    return -1;

  struct thread *curr = thread_current();
  struct file **fdt = curr->fd_table;

  while (curr->fd_idx < FD_COUNT_LIMIT && fdt[curr->fd_idx])
    curr->fd_idx++;

  if (curr->fd_idx >= FD_COUNT_LIMIT) {
    file_close (f);
    return -1;
  }
  fdt[curr->fd_idx] = f;

  return curr->fd_idx;
}

static void
check_addr (const char *f_addr) {
  if (!is_user_vaddr(f_addr) || f_addr == NULL || !pml4_get_page(thread_current()->pml4, f_addr))
    exit(-1);
}

static bool
remove (const char *file) {
  check_addr(file);
  return filesys_remove(file);
}

static int
filesize (int fd) {
  if (fd <= 1)
    return -1;

  struct thread *curr = thread_current ();
  struct file *f = curr->fd_table[fd];

  if (f == NULL)
    return -1;

  int size = file_length(f);
  return size;
}

static int
read (int fd, void *buffer, unsigned length) {
  check_addr(buffer);
  if (fd > FD_COUNT_LIMIT || fd == STDOUT_FILENO || fd < 0)
    return -1;

  struct thread *curr = thread_current ();
  struct file *f = curr->fd_table[fd];

  if (f == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  int read_size = file_read(f, buffer, length);
  lock_release(&filesys_lock);

  return read_size;
}

static int
write (int fd, const void *buffer, unsigned length) {
  check_addr(buffer);
  if (fd > FD_COUNT_LIMIT || fd <= 0)
    return -1;

  if (fd == 1) {
    putbuf(buffer, length);
    return 0;
  }
  else {
    struct thread *curr = thread_current ();
    struct file *f = curr->fd_table[fd];

    if (f == NULL)
      return -1;

    lock_acquire(&filesys_lock);
    int write_size = file_write(f, buffer, length);
    lock_release(&filesys_lock);

    return write_size;
  }
}

static void
seek (int fd, unsigned position) {
  struct thread *curr = thread_current ();
  struct file *f = curr->fd_table[fd];

  if (!is_kernel_vaddr(f))
    exit(-1);

  file_seek(f, position);
}

static unsigned
tell (int fd) {
  struct thread *curr = thread_current ();
  struct file *f = curr->fd_table[fd];

  if (!is_kernel_vaddr(f))
    exit(-1);

  return file_tell(f);
}

void
close (int fd) {
  if (fd <= 1)
    return;

  struct thread *curr = thread_current ();
  struct file *f = curr->fd_table[fd];

  if (f == NULL)
    return;

  curr->fd_table[fd] = NULL;
  file_close(f);
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
void *
mmap (void *addr, size_t length, int writable, int fd, off_t offset) {
	return;
}

/* 아직 매핑되지 않은 동일한 프로세스에 의해 이전에 호출된 mmap에 의해 반환된 가상 주소 범위 addr에 대한 매핑을 해제합니다.

 * 모든 매핑은 프로세스가 종료될 때 묵시적으로 해제됩니다.
 * 매핑이 해제되면 명시적 또는 묵시적으로 프로세스에 의해 기록된 모든 페이지가 파일에 기록되어야 하며,
 * 기록되지 않은 페이지는 그렇지 않아야 합니다. 그런 다음 페이지가 프로세스의 가상 페이지 목록에서 제거됩니다.

 * 파일을 닫거나 제거하면 해당 파일의 매핑이 해제되지 않습니다.
 * 한 번 생성된 매핑은 munmap이 호출되거나 프로세스가 종료될 때까지 유효합니다.
 * 유닉스 규칙을 따릅니다. 자세한 내용은 열린 파일 제거를 참조하십시오.
 * 각 매핑에 대해 파일을 별도로 참조하고 독립적으로 얻으려면 file_reopen 함수를 사용해야 합니다.

 * 동일한 파일을 두 개 이상의 프로세스가 매핑하는 경우 일관된 데이터를 볼 필요가 없습니다
 * 유닉스는 두 매핑이 동일한 물리적 페이지를 공유하도록 만들고 mmap 시스템 호출에는
 * 페이지를 공유 또는 개인(쓰기 시 복사)로 지정할 수 있는 인수도 있습니다.

 * vm_file_init 및 vm_file_initializer를 vm/vm.c에서 필요에 따라 수정할 수 있습니다. */
void
munmap (void *addr) {
	return;
}