#define _DEFAULT_SOURCE

#include "mini_alloc.h"
#include "mini_alloc_internal.h"

#include <limits.h>
#include <stdint.h>
#include <unistd.h>

block_meta_t *g_base = NULL;

int mm_is_valid_block(const block_meta_t *b) {
    return (b != NULL) && (b->magic == BLOCK_MAGIC) && (b->size != 0);
}

static block_meta_t *find_free_block(block_meta_t **last, size_t size) {
    block_meta_t *cur = g_base;
    *last = NULL;

    while (cur != NULL) {
        if (!mm_is_valid_block(cur)) {
            mm_debug_log("find_free_block: encountered invalid block %p", (void *)cur);
            return NULL;
        }

        if (cur->free && cur->size >= size) { return cur; }

        *last = cur;
        cur = cur->next;
    }

    return NULL;
}

static block_meta_t *request_space(block_meta_t *last, size_t size) {
    void *mem = sbrk((intptr_t)(META_SIZE + size));
    if (mem == (void *)-1) { return NULL; }

    block_meta_t *b = (block_meta_t *)mem;
    b->size  = size;
    b->next  = NULL;
    b->prev  = last;
    b->magic = BLOCK_MAGIC;
    b->free  = 0;

    if (last != NULL) {
        last->next = b;
    } else {
        g_base = b;
    }

    mm_debug_log("request_space: new block=%p size=%zu", (void *)b, size);
    return b;
}

void *mm_malloc(size_t size) {
    if (size == 0) { return NULL; }

    size = ALIGN8(size);
    mm_debug_log("mm_malloc(%zu)", size);

    if (size > (size_t)(INTPTR_MAX - (intptr_t)META_SIZE)) {
        mm_debug_log("mm_malloc: requested size too large (%zu)", size);
        return NULL;
    }

    block_meta_t *last = NULL;
    block_meta_t *b = find_free_block(&last, size);

    if (b != NULL) {
        b->free = 0;
        mm_debug_log("mm_malloc: reusing free block=%p size=%zu", (void *)b, b->size);
        return (void *)(b + 1);
    }

    b = request_space(last, size);
    if (b == NULL) {
        mm_debug_log("mm_malloc: sbrk failed");
        return NULL;
    }

    return (void *)(b + 1);
}

void mm_free(void *ptr) {
    if (ptr == NULL) { return; }

    block_meta_t *b = ((block_meta_t *)ptr) - 1;

    if (!mm_is_valid_block(b)) {
        mm_debug_log("mm_free: invalid pointer %p", ptr);
        return;
    }

    if (b->free) {
        mm_debug_log("mm_free: double free or duplicate free on block=%p", (void *)b);
        return;
    }

    b->free = 1;
    mm_debug_log("mm_free: freed block=%p size=%zu", (void *)b, b->size);
}
