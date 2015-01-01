/*
handler of inter process communication(a typical example is a process waiting for process to exit)
using a pipeline model (pretty similar to reading/writing to the terminal),
a read from an empty pipe will block the caller until another process write something to the pipe,
a read from a non-empty pipe will not block the caller
note that reading/writing can happen in any order (read->write or write->read)
 */

#include <list.h>
#include <debug.h>
#include "threads/synch.h"
#include "userprog/ipc.h"
#include "threads/malloc.h"

// pipe is identified by a pipe name and a ticket number.

struct write {
    int ticket;
    char *pipe_name;
    int msg; // return value to the reader, supporting only int for now since it's sufficient for the current use cases, and relieve from memory management
    struct list_elem elem;
};

struct read {
    int ticket;
    char *pipe_name;
    struct semaphore sema; // the semaphore sync primitive is used to implement the blocking
    struct list_elem elem;
};

static struct list write_list;
static struct list read_list;

void ipc_init(void){
    list_init(&read_list);
    list_init(&write_list);
}

int ipc_pipe_read(char * pipe_name, int ticket){
  // check whether there is already something written that correspond to the read 
  struct list_elem *e;
  for (e = list_begin (&write_list); e != list_end (&write_list); e = list_next (e))
  {
    struct write *w = list_entry (e, struct write, elem);
    if(w->pipe_name == pipe_name && ticket == w->ticket){
      list_remove(e);
      int msg = w->msg;
      free(w);
      return msg;
    }
  }
  // add an entry to the read list and block the caller
  struct read *r = malloc(sizeof(struct read));
  sema_init(&r->sema, 0);
  r->ticket = ticket;
  r->pipe_name = pipe_name;
  list_push_back(&read_list, &r->elem);
  sema_down(&r->sema);
  // something written, re-parse the list again
  // TODO: possibility for code reuse
  for (e = list_begin (&write_list); e != list_end (&write_list); e = list_next (e))
  {
    struct write *w = list_entry (e, struct write, elem);
    if(w->pipe_name == r->pipe_name && r->ticket == w->ticket){
      list_remove(e);
      list_remove(&r->elem);
      int msg = w->msg;
      free(w);
      free(r);
      return msg;
    }
  }
  NOT_REACHED ();
}

void ipc_pipe_write(char *pipe_name, int ticket, int msg){
  struct write *w= malloc(sizeof(struct write));
  w->ticket = ticket;
  w->pipe_name = pipe_name;
  w->msg = msg;
  list_push_back(&write_list, &w->elem);

  // check whether there is someone waiting for the write to up the semaphore
  struct list_elem *e;
  for (e = list_begin (&read_list); e != list_end (&read_list); e = list_next (e))
  {
    struct read *r = list_entry (e, struct read, elem);
    if(r->pipe_name == pipe_name && r->ticket == w->ticket){
      sema_up(&r->sema);
    }
  }
}
