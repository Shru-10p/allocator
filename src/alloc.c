#define _DEFAULT_SOURCE

#include "alloc.h"
#include "internal.h"

#include <limits.h>
#include <stdint.h>
#include <unistd.h>

block_meta_t *g_base = NULL;

int is_valid_block(const block_meta_t *b) {
    return (b != NULL) && (b->magic == BLOCK_MAGIC) && (b->size != 0);
}

static block_meta_t *find_free_block(block_meta_t **last, size_t size) {
    block_meta_t *cur = g_base;
    *last = NULL;

    while (cur != NULL) {
        if (!is_valid_block(cur)) {
            debug_log("find_free_block: encountered invalid block %p", (void *)cur);
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

    debug_log("request_space: new block=%p size=%zu", (void *)b, size);
    return b;
}

void *my_malloc(size_t size) {
    if (size == 0) { return NULL; }

    size = ALIGN8(size);
    debug_log("malloc(%zu)", size);

    if (size > (size_t)(INTPTR_MAX - (intptr_t)META_SIZE)) {
        debug_log("malloc: requested size too large (%zu)", size);
        return NULL;
    }

    block_meta_t *last = NULL;
    block_meta_t *b = find_free_block(&last, size);

    if (b != NULL) {
        b->free = 0;
        debug_log("malloc: reusing free block=%p size=%zu", (void *)b, b->size);
        return (void *)(b + 1);
    }

    b = request_space(last, size);
    if (b == NULL) {
        debug_log("malloc: sbrk failed");
        return NULL;
    }

    return (void *)(b + 1);
}

void my_free(void *ptr) {
    if (ptr == NULL) { return; }

    block_meta_t *b = ((block_meta_t *)ptr) - 1;

    if (!is_valid_block(b)) {
        debug_log("free: invalid pointer %p", ptr);
        return;
    }

    if (b->free) {
        debug_log("free: double free or duplicate free on block=%p", (void *)b);
        return;
    }

    b->free = 1;
    debug_log("free: freed block=%p size=%zu", (void *)b, b->size);
}
