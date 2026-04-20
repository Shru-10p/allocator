#ifndef ALLOC_INTERNAL_H
#define ALLOC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#define ALIGN_SIZE 8
#define ALIGN(size) (((size) + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1))
#define BLOCK_MAGIC 0xC0FFEE42u
#define MMAP_THRESHOLD 32768ULL // 32KiB (glibc malloc uses 128 KiB)

typedef struct block_meta {
    size_t size;
    size_t mapping_size; // only used for mmap blocks
    struct block_meta *next;
    struct block_meta *prev;
    uint32_t magic;
    uint8_t free;
    uint8_t use_mmap;
} block_meta_t;

#define META_SIZE (sizeof(block_meta_t))

extern block_meta_t *g_base;

int is_valid_block(const block_meta_t *b);
void debug_log(const char *fmt, ...);

#endif // ALLOC_INTERNAL_H
