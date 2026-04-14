#ifndef MINI_ALLOC_H
#define MINI_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *mm_malloc(size_t size);
void  mm_free(void *ptr);

void   mm_set_debug(int enabled);
void   mm_debug_print_heap(void);
int    mm_validate_heap(void);
size_t mm_block_count(void);

#ifdef __cplusplus
}
#endif

#endif // MINI_ALLOC_H
