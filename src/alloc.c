#define _DEFAULT_SOURCE

#include "alloc.h"
#include "./internal/internal.h"

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
            return NULL; // currently treats invalid block as end of list; could lead to data loss if later blocks are valid. -- need to fix.
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

static void split_block(block_meta_t *b, size_t requested){
    if (b == NULL) { return; }
    if (!is_valid_block(b)) {
        debug_log("split_block: invalid block %p", (void *)b);
        return;
    }
    if (!(b->free)) {
        debug_log("split_block: block %p is not free", (void *)b);
        return;
    }
    if (requested > b->size) {
        debug_log("split_block: requested size %zu is larger than block size %zu", requested, b->size);
        return;

    }
    size_t remaining_size = b->size - requested;
    if (remaining_size < META_SIZE + 8ULL) { // remaining block must be useable.
        debug_log("split_block: remaining size %zu is too small to split", remaining_size);
        return;
    }

    block_meta_t *old_next = b->next;

    block_meta_t *remainder = (block_meta_t *)((char *)(b + 1) + requested);
    remainder->size = remaining_size - META_SIZE;
    remainder->next = old_next;
    remainder->prev = b;
    remainder->magic = BLOCK_MAGIC;
    remainder->free = 1;

    if (old_next != NULL) {
        old_next->prev = remainder;
    }

    b->size = requested;
    b->next = remainder;
    debug_log("split_block: split block=%p into allocated size=%zu and free remainder=%p size=%zu", (void *)b, requested, (void *)remainder, remainder->size);
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
        split_block(b, size);
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
