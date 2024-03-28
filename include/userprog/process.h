#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#ifdef VM
/* ------ Project 3 ------ */
typedef struct {
  struct file *file;
  size_t read_bytes;
  off_t ofs;
} file_info;
/* ----------------------- */
#endif

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* Project 2 - Argument Passing */
static void argument_passing (struct intr_frame *if_, int argv_cnt, char **argv_list);

#endif /* userprog/process.h */
