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
// #include "threads/vaddr.h"
/* ------------------------ */

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* ------ Project 2 ------ */
struct lock filesys_lock;

static void check_addr(const char *f_addr);

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
  //? 인자 - %rdi, $rsi, %rdx, %r10, %r9, %r8
  // printf("######## DBG ######## rdi = { %s }\n", f->R.rdi);
  // printf("######## DBG ######## rsi = { %s }\n", f->R.rsi);
  // printf("######## DBG ######## rdx = { %s }\n", f->R.rdx);
  // printf("######## DBG ######## r10 = { %s }\n", f->R.r10);
  // printf("######## DBG ######## r9 = { %s }\n", f->R.r9);
  // printf("######## DBG ######## r8 = { %s }\n", f->R.r8);

  int sys_number = f->R.rax;

  switch (sys_number) {

    case SYS_HALT:          /* 0 Halt the operating system. */
      halt ();
      break;

    case SYS_EXIT:          /* 1 Terminate this process. */
      exit (f->R.rdi);
      break;

    case SYS_FORK:          /* 2 Clone current process. */
      f->R.rax = fork ((char *)f->R.rdi, f);
      break;

    case SYS_EXEC:          /* 3 Switch current process. */
      f->R.rax = exec ((char *)f->R.rdi);
      break;

    case SYS_WAIT:          /* 4 Wait for a child process to die. */
      f->R.rax = wait (f->R.rdi);
      break;

    case SYS_CREATE:        /* 5 Create a file. */
      f->R.rax = create((char *)f->R.rdi, f->R.rsi);
      break;

    case SYS_REMOVE:
      printf("Performing syscall 6\n");
      // syscall 6 처리
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
      f->R.rax = write (f->R.rdi, (void *)f->R.rsi, f->R.rdx);
      break;

    case SYS_SEEK:
      printf("Performing syscall 11\n");
      // syscall 11 처리
      break;

    case SYS_TELL:
      printf("Performing syscall 12\n");
      // syscall 12 처리
      break;

    case SYS_CLOSE:         /* 13 Close a file. */
      close(f->R.rsi);
      break;

    default:
      printf ("system call!\n");
      thread_exit ();
      // printf("Unknown syscall number\n");
      // break;
  }
}

/*
  TODO: SYS_REMOVE,                  6 Delete a file.
  TODO: SYS_SEEK,                    11 Change position in a file.
  TODO: SYS_TELL,                    12 Report current position in a file.
*/

static void
halt (void) {

  power_off ();
}

static void
exit (int status) {
  struct thread *curr = thread_current ();
  curr->exit_status = status;

  thread_exit ();
}

static pid_t
fork(const char *thread_name, struct intr_frame *f) {
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

  NOT_REACHED();
  return 0;
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
open(const char *file) {
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
check_addr(const char *f_addr) {
  if (!is_user_vaddr(f_addr) || f_addr == NULL || !pml4_get_page(thread_current()->pml4, f_addr))
    exit(-1);
}

// bool remove (const char *file);
static int
filesize (int fd) {
  int size = -1;

  if (fd <= 1)
    return size;

  struct thread *curr = thread_current ();
  struct file *f = curr->fd_table[fd];

  if (f == NULL)
    return size;

  size = file_length(f);
  return size;
}

static int
read (int fd, void *buffer, unsigned length) {
  int read_size = -1;

  check_addr(buffer);
  if (fd > FD_COUNT_LIMIT || fd == STDOUT_FILENO || fd < 0)
    return read_size;

  struct thread *curr = thread_current ();
  struct file *f = curr->fd_table[fd];

  if (f == NULL)
    return read_size;

  lock_acquire (&filesys_lock);
  read_size = file_read(f, buffer, length);
  lock_release (&filesys_lock);

  return read_size;
}

static int
write (int fd, const void *buffer, unsigned length) {
  int write_size = -1;

  check_addr(buffer);
  if (fd > FD_COUNT_LIMIT || fd <= 0)
    return write_size;

  if (fd == 1) {
    putbuf (buffer, length);
    return 0;
  }
  else {
    struct thread *curr = thread_current ();
    struct file *f = curr->fd_table[fd];

    if (f == NULL)
      return write_size;

    lock_acquire (&filesys_lock);
    write_size = file_write(f, buffer, length);
    lock_release (&filesys_lock);
  }
  return write_size;

}
// void seek (int fd, unsigned position);
// unsigned tell (int fd);
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