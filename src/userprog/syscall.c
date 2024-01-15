#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "devices/input.h"


static size_t ptr_size = sizeof(void*);

static void syscall_handler (struct intr_frame *);

static void
syscall_halt(struct intr_frame *f UNUSED) {
  shutdown_power_off();
}

static void
syscall_exit(struct intr_frame *f) {
  int exit_code = *(int*)(f->esp + sizeof(void*));

  thread_exit();
}

static void
syscall_write(struct intr_frame *f) {
  int fd = *(int*)(f->esp + ptr_size);
  char *buf = *(char**)(f->esp + 2*ptr_size);
  int size = *(int*)(f->esp + 3*ptr_size);

  if(fd == 1) {
    putbuf(buf,size);
    f->eax = size;
  }
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int syscall_type = *(int*)f->esp;
  switch(syscall_type) {
  case SYS_HALT:
    syscall_halt(f);
    break;
  case SYS_EXIT:
    syscall_exit(f);
    break;
  case SYS_WRITE:
    syscall_write(f);
    break;
  }
}
