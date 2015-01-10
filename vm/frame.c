
#include "vm/frame.h"
#include <list.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/process.h"

struct frame{
    void * addr;
    pid_t owner; //owner process
    struct list_elem elem;
};

static struct list frame_list;

void frame_init(){
    list_init(&frame_list);
}

void *frame_get_page (bool zero_it){
    enum palloc_flags flags = PAL_ZERO;
    if(zero_it)
        flags |= PAL_ZERO;
    void *page = palloc_get_page(flags);
    if(page == NULL){
        //evict another page
    }
    struct frame *f = malloc(sizeof(struct frame));
    f->addr = page;
    f->owner = process_pid();
    list_push_back(&frame_list, &f->elem);

    return f->addr;
}

void frame_free_page (void * addr){
  struct list_elem *e;
  struct frame *f = NULL;
  for (e = list_begin (&frame_list); e != list_end (&frame_list); e = list_next (e))
  {
    struct frame *tmp = list_entry (e, struct frame, elem);
    if(tmp->addr == addr){
      f = tmp;
      break;
    }
  }

  if (f != NULL){
    list_remove(&f->elem);
    palloc_free_page(f->addr);
    free(f);
  }
}