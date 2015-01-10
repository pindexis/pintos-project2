#include "vm/page.h"
#include <hash.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include <stdio.h>
struct page * page_lookup (const void *addr);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);

struct page{
    struct hash_elem hash_elem; /* Hash table element. */
    uint32_t *addr;                 /* Virtual address. */
    
    pagefault_handler *pagefault_handler;
    void* pagefault_handler_args;
};

/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct page *
page_lookup (const void *addr)
{
    
  struct page p;
  struct hash_elem *e;

  p.addr = addr;
  e = hash_find (&process_current()->spt, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

void page_lazy_load(const void *upage, pagefault_handler *handler, void *args){
    ASSERT (pg_ofs (upage) == 0);
    // if it already exists
    if(page_lookup(upage) != NULL){
        printf("page already exists %p\n", upage);
        //not sure what to do
        //TODO:revisit this
        return;
    }
    //printf("adding %p\n", upage);
    struct page *page = malloc(sizeof(struct page));
    if (page == NULL){
        PANIC("cannot allocate memory");
    }
    page->addr = (uint32_t *) upage;
    page->pagefault_handler = handler;
    page->pagefault_handler_args = args;
    hash_insert(&process_current()->spt, &page->hash_elem);
}
 
bool page_handle_page_fault(const void *addr){
    uint32_t * upage = (uint32_t *) pg_addr(addr);
    if(page_lookup(upage) == NULL){
        printf("page not found %p\n", upage);
        // invalid access
        return false;
    }
    // TODO: validate other forms of invalid access(example; write to read only page)
    struct page *page = page_lookup(upage);
    page->pagefault_handler(upage, page->pagefault_handler_args);
    hash_delete(&process_current()->spt, &page->hash_elem);
    return true;
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}
void page_init(void){
    hash_init (&process_current()->spt, page_hash, page_less, NULL);
}