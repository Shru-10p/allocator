#ifndef ALLOC_INTERNAL_H
#define ALLOC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#define ALIGN_SIZE 8
#define ALIGN(size) (((size) + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1))
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

int is_valid_block(const block_meta_t *b);
void debug_log(const char *fmt, ...);

#endif // ALLOC_INTERNAL_H
