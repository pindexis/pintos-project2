#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"

typedef int pid_t;
static int (*syscall_handlers[20]) (struct intr_frame *); /* Array of syscall functions */


/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  if(!is_user_vaddr(uaddr))
    return -1;
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  if(!is_user_vaddr(udst))
    return false;
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}


static bool is_valid_pointer(void * esp, uint8_t argc){
  uint8_t i = 0;
  for (; i < argc; ++i)
  {
    if (get_user(((uint8_t *)esp)+i) == -1){
      return false;
    }
  }
  return true;
}

static bool is_valid_string(void * str)
{
  int ch=-1;
  while((ch=get_user((uint8_t*)str++))!='\0' && ch!=-1);
    if(ch=='\0')
      return true;
    else
      return false;
}


static void
syscall_exit (int status)
{
  thread_exit (status);
}

static void
syscall_halt(void)
{
  shutdown();
}

static pid_t
syscall_exec(const char *file_name)
{
  return process_execute (file_name);
}

static int
syscall_wait (pid_t pid)
{
  return process_wait(pid);
}

static bool syscall_create (const char *file_name, unsigned initial_size)
{
  return filesys_create (file_name, initial_size);
}
static bool syscall_remove (const char *file_name)
{
  return filesys_remove (file_name);
}


static int
syscall_filesize_wrapper(struct intr_frame *f)
{
  if (!is_valid_pointer(f->esp +4, 4)){
    return -1;
  }
  int fd = *(int *)(f->esp + 4);
  f->eax = process_filesize(fd);
  return 0;
}

static int
syscall_seek_wrapper(struct intr_frame *f)
{
  if (!is_valid_pointer(f->esp +4, 8)){
    return -1;
  }
  int fd = *(int *)(f->esp + 4);
  unsigned pos = *(unsigned *)(f->esp + 8);
  process_seek(fd, pos);
  return 0;
}

static int
syscall_tell_wrapper(struct intr_frame *f)
{
  if (!is_valid_pointer(f->esp +4, 4)){
    return -1;
  }
  int fd = *(int *)(f->esp + 4);
  f->eax = process_tell(fd);
  return 0;
}

static int
syscall_create_wrapper(struct intr_frame *f)
{
  if (
    !is_valid_pointer(f->esp +4, 4) ||
    !is_valid_string(*(char **)(f->esp + 4)) ||
    !is_valid_pointer(f->esp +8, 4)){
    return -1;
  }
  char *str = *(char **)(f->esp + 4);
  unsigned size = *(int *)(f->esp + 8);
  f->eax = syscall_create(str, size);
  return 0;
}

static int
syscall_remove_wrapper(struct intr_frame *f)
{
  if (!is_valid_pointer(f->esp +4, 4) || !is_valid_string(*(char **)(f->esp + 4))){
    return -1;
  }
  char *str = *(char **)(f->esp + 4);
  f->eax = syscall_remove(str);
  return 0;
}

static int
syscall_open_wrapper(struct intr_frame *f)
{
  if (!is_valid_pointer(f->esp +4, 4) || !is_valid_string(*(char **)(f->esp + 4))){
    return -1;
  }
  char *str = *(char **)(f->esp + 4);
  f->eax = process_open(str);
  return 0;
}

static int
syscall_close_wrapper(struct intr_frame *f)
{
  if (!is_valid_pointer(f->esp +4, 4)){
    return -1;
  }
  int fd = *(int *)(f->esp + 4);
  process_close(fd);
  return 0;
}

static int
syscall_exit_wrapper(struct intr_frame *f)
{
  int status;
  if (is_valid_pointer(f->esp + 4, 4))
    status = *((int*)f->esp+1);
  else
    return -1;
  syscall_exit(status);
  return 0;
}

static int
syscall_halt_wrapper(struct  intr_frame *f UNUSED)
{
  syscall_halt();
  return 0;
}

static int
syscall_wait_wrapper(struct  intr_frame *f)
{
  pid_t pid;
  if (is_valid_pointer(f->esp + 4, 4))
    pid = *((int*)f->esp+1);
  else
    return -1;
  f->eax = syscall_wait(pid);
  return 0;
}

static int
syscall_exec_wrapper(struct  intr_frame *f)
{
  if (!is_valid_pointer(f->esp +4, 4) || !is_valid_string(*(char **)(f->esp + 4))){
    return -1;
  }
  char *str = *(char **)(f->esp + 4);
  // make sure the command string can fit into a page
  if ( strlen(str) >= PGSIZE ){
    printf("very large strings(>PGSIZE) are not supported\n");
    return -1;
  }
  // non empty string and it does not start with a space(args delimiter)
  if (strlen(str) == 0 || str[0] == ' '){
    printf("the command string should be non-empty and doesn't start with a space %s\n", str);
    return -1;
  }
  f->eax = syscall_exec(str);
  return 0;
}

static int
syscall_write_wrapper(struct  intr_frame *f)
{
  if (!is_valid_pointer(f->esp + 4, 12)){
    return -1;
  }
  int fd = *(int *)(f->esp + 4);
  void *buffer = *(char**)(f->esp + 8);
  unsigned size = *(unsigned *)(f->esp + 12);
  if (!is_valid_pointer(buffer, 1) || !is_valid_pointer(buffer + size,1)){
    return -1;
  }
  int written_size = process_write(fd, buffer, size);
  f->eax = written_size;
  return 0;
}

static int
syscall_read_wrapper(struct  intr_frame *f)
{
  if (!is_valid_pointer(f->esp + 4, 12)){
    return -1;
  }
  int fd = *(int *)(f->esp + 4);
  void *buffer = *(char**)(f->esp + 8);
  unsigned size = *(unsigned *)(f->esp + 12);
  if (!is_valid_pointer(buffer, 1) || !is_valid_pointer(buffer + size,1)){
    return -1;
  }
  int written_size = process_read(fd, buffer, size);
  f->eax = written_size;
  return 0;
}

static void
kill_program(void)
{
  thread_exit(-1);
}

static void
syscall_handler (struct intr_frame *f)
{

  if (!is_valid_pointer(f->esp, 4)){
    kill_program();
    return;
  }
  int syscall_num = * (int *)f->esp;

  if (syscall_num < 0 || syscall_num >= 20){
    kill_program();
    return;
  }
  if(syscall_handlers[syscall_num](f) == -1){
    kill_program();
    return;
  }
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  syscall_handlers[SYS_EXIT] = &syscall_exit_wrapper;
  syscall_handlers[SYS_WRITE] = &syscall_write_wrapper;
  syscall_handlers[SYS_EXEC] = &syscall_exec_wrapper;
  syscall_handlers[SYS_HALT] = &syscall_halt_wrapper;
  syscall_handlers[SYS_WAIT] = &syscall_wait_wrapper;
  syscall_handlers[SYS_CREATE] = &syscall_create_wrapper;
  syscall_handlers[SYS_REMOVE] = &syscall_remove_wrapper;
  syscall_handlers[SYS_OPEN] = &syscall_open_wrapper;
  syscall_handlers[SYS_CLOSE] = &syscall_close_wrapper;
  syscall_handlers[SYS_READ] = &syscall_read_wrapper;
  syscall_handlers[SYS_FILESIZE] = &syscall_filesize_wrapper;
  syscall_handlers[SYS_SEEK] = &syscall_seek_wrapper;
  syscall_handlers[SYS_TELL] = &syscall_tell_wrapper;
}

