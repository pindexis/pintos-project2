/*
 Frame is a layer of abstraction over page allocator that will contains paging logic (frame eviction policy, swapping)
 I leaned toward the approach of calling frame_alloc from a user context(process.c and system calls) to allocate pages, rather than incorporating it into palloc (IE process->Frame_alloc->Palloc rather than process->Palloc->Frame_update)because I think this better separates concerns(although it involves updating process code to use the new allocator, and can introduces some overhead since multiple lists need to be maintained).
 */

#include <stdbool.h>

void frame_init(void);
void *frame_get_page (bool zero_it);
void frame_free_page (void *);