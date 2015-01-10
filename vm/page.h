#include <stdbool.h>

// handler of the pagefault
typedef void pagefault_handler(void *addr, void *args);

void page_init(void);
bool page_handle_page_fault(const void *addr);
void page_lazy_load(const void *addr, pagefault_handler *handler, void *args);


