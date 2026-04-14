#ifndef MINI_ALLOC_INTERNAL_H
#define MINI_ALLOC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#define ALIGN8(x) (((x) + 7ULL) & ~7ULL)
#define BLOCK_MAGIC 0xC0FFEE42u

typedef struct block_meta {
    size_t size;
    struct block_meta *next;
    struct block_meta *prev;
    uint32_t magic;
    uint8_t free;
} block_meta_t;

#define META_SIZE (sizeof(block_meta_t))

extern block_meta_t *g_base;

int mm_is_valid_block(const block_meta_t *b);
void mm_debug_log(const char *fmt, ...);

#endif
