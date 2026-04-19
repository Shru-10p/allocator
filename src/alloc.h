#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *my_malloc(size_t size);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);
void  my_free(void *ptr);

void   set_debug(int enabled);
void   debug_print_heap(void);
int    validate_heap(void);
size_t block_count(void);

#ifdef __cplusplus
}
#endif

#endif // ALLOC_H
