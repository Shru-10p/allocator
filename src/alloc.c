#define _DEFAULT_SOURCE

#include "alloc.h"
#include "./internal/internal.h"

#include <limits.h>
#include <sys/mman.h> // for mmap, munmap
#include <stdint.h>
#include <string.h> // for memset
#include <unistd.h>

block_meta_t *g_base = NULL;

int is_valid_block(const block_meta_t *b) {
    return (b != NULL) && (b->magic == BLOCK_MAGIC) && (b->size != 0);
}

static size_t total_block_size(size_t payload_size) {
    return META_SIZE + payload_size;
}

static size_t round_up_to_multiple(size_t value, size_t multiple) {
    size_t remainder = value % multiple;

    if (remainder == 0) {
        return value;
    }

    size_t increment = multiple - remainder;
    if (value > SIZE_MAX - increment) {
        return 0;
    }

    return value + increment;
}

static void init_block_meta(
    block_meta_t *b,
    size_t size,
    size_t mapping_size,
    block_meta_t *prev,
    block_meta_t *next,
    uint8_t use_mmap
) {
    b->size = size;
    b->mapping_size = mapping_size;
    b->next = next;
    b->prev = prev;
    b->magic = BLOCK_MAGIC;
    b->free = 0;
    b->use_mmap = use_mmap;
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

static block_meta_t *request_space(block_meta_t *last, size_t size, uint8_t use_mmap) {
    void *mem;
    size_t mapping_size = 0;

    if (size > SIZE_MAX - META_SIZE) {
        return NULL;
    }

    if (use_mmap) {
        size_t total_size = META_SIZE + size;
        mapping_size = round_up_to_multiple(total_size, MMAP_THRESHOLD);
        if (mapping_size == 0) {
            return NULL;
        }

        mem = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) {
            return NULL;
        }
    } else {
        mem = sbrk((intptr_t)(META_SIZE + size));
        if (mem == (void *)-1) {
            return NULL;
        }
    }

    block_meta_t *b = (block_meta_t *)mem;
    init_block_meta(b, size, mapping_size, use_mmap ? NULL : last, NULL, use_mmap);

    if (!use_mmap) {
        if (last != NULL) {
            last->next = b;
        } else {
            g_base = b;
        }
        debug_log("request_space: new sbrk block=%p size=%zu", (void *)b, size);
    } else {
        debug_log("request_space: new mmap block=%p size=%zu mapping_size=%zu", (void *)b, size, mapping_size);
    }

    return b;
}

static void split_block(block_meta_t *b, size_t requested){
    if (b == NULL) { return; }
    if (!is_valid_block(b)) {
        debug_log("split_block: invalid block %p", (void *)b);
        return;
    }
    if (b->use_mmap) {
        debug_log("split_block: mmap block %p is not split", (void *)b);
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
    init_block_meta(remainder, remaining_size - META_SIZE, 0, b, old_next, 0);
    remainder->free = 1;

    if (old_next != NULL) {
        old_next->prev = remainder;
    }

    b->size = requested;
    b->next = remainder;
    debug_log("split_block: split block=%p into allocated size=%zu and free remainder=%p size=%zu", (void *)b, requested, (void *)remainder, remainder->size);
}

static void coalesce(block_meta_t *b) {
    if (b == NULL) { return; }
    if (!is_valid_block(b)) {
        debug_log("coalesce: invalid block %p", (void *)b);
        return;
    }
    if (!b->free) {
        debug_log("coalesce: block %p is not free", (void *)b);
        return;
    }
    // coalesce backward
    while (b->prev != NULL) {
        block_meta_t *prev = b->prev;
        if (!is_valid_block(prev)) {
            debug_log("coalesce: invalid previous block %p", (void *)prev);
            break;
        }
        if (!prev->free) { break; }

        debug_log("coalesce: coalescing with previous block=%p size=%zu", (void *)prev, prev->size);
        prev->size += META_SIZE + b->size;
        prev->next = b->next;
        if (b->next != NULL) { b->next->prev = prev; }
        b = prev;
    }
    // coalesce forward
    while (b->next != NULL) {
        block_meta_t *next = b->next;
        if (!is_valid_block(next)) {
            debug_log("coalesce: invalid next block %p", (void *)next);
            break;
        }
        if (!next->free) { break; }

        debug_log("coalesce: coalescing with next block=%p size=%zu", (void *)next, next->size);
        b->size += META_SIZE + next->size;
        b->next = next->next;
        if (next->next != NULL) { next->next->prev = b; }
    }
}

void *my_malloc(size_t size) {
    if (size == 0) { return NULL; }

    size = ALIGN(size); // align to ALIGN_SIZE bytes
    debug_log("malloc(%zu)", size);

    if (size > (size_t)(INTPTR_MAX - (intptr_t)META_SIZE)) {
        debug_log("malloc: requested size too large (%zu)", size);
        return NULL;
    }

    if (size >= MMAP_THRESHOLD) {
        block_meta_t *mmap_block = request_space(NULL, size, 1);
        if (mmap_block == NULL) {
            debug_log("malloc: mmap failed for size %zu", size);
            return NULL;
        }

        return (void *)(mmap_block + 1);
    }

    block_meta_t *last = NULL;
    block_meta_t *b = find_free_block(&last, size);

    if (b != NULL) {
        split_block(b, size);
        b->free = 0;
        debug_log("malloc: reusing free block=%p size=%zu", (void *)b, b->size);
        return (void *)(b + 1);
    }

    b = request_space(last, size, 0);
    if (b == NULL) {
        debug_log("malloc: sbrk failed");
        return NULL;
    }

    return (void *)(b + 1);
}

void *my_calloc(size_t nmemb, size_t size){
    if (nmemb == 0 || size == 0) { return NULL; }

    if (nmemb > SIZE_MAX / size) {
        debug_log("calloc: requested size too large (nmemb=%zu, size=%zu)", nmemb, size);
        return NULL;
    }

    size_t requested_total = nmemb * size;
    void *ptr = my_malloc(requested_total);
    if (ptr == NULL) {
        debug_log("calloc: malloc failed for requested=%zu", requested_total);
        return NULL;
    }

    block_meta_t *b = ((block_meta_t *)ptr) - 1;
    size_t payload_size = b->size;

    memset(ptr, 0, requested_total);
    debug_log(
        "calloc: block=%p nmemb=%zu size=%zu requested=%zu payload=%zu total_block=%zu",
        ptr,
        nmemb,
        size,
        requested_total,
        payload_size,
        total_block_size(payload_size)
    );
    return ptr;
}

void *my_realloc(void *ptr, size_t size) {
    size = ALIGN(size);
    if (ptr == NULL) {
        return my_malloc(size);
    }
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    block_meta_t *b = ((block_meta_t *)ptr) - 1;

    if (!is_valid_block(b)) {
        debug_log("realloc: invalid pointer %p", ptr);
        return NULL;
    }

    if (b->use_mmap) {
        void *new_ptr = my_malloc(size);
        size_t copy_size;
        size_t old_payload = b->size;

        if (new_ptr == NULL) {
            debug_log("realloc: mmap block copy allocation failed for new size %zu", size);
            return NULL;
        }

        copy_size = b->size < size ? b->size : size;
        memcpy(new_ptr, ptr, copy_size);
        my_free(ptr);

        debug_log(
            "realloc: moved mmap block old_ptr=%p new_ptr=%p requested=%zu old_payload=%zu new_payload=%zu",
            ptr,
            new_ptr,
            size,
            old_payload,
            ((block_meta_t *)new_ptr - 1)->size
        );
        return new_ptr;
    }

    if (b->size >= size) {
        size_t old_payload = b->size;
        split_block(b, size);
        debug_log(
            "realloc: resized in place block=%p requested=%zu old_payload=%zu new_payload=%zu old_total=%zu new_total=%zu",
            (void *)b,
            size,
            old_payload,
            b->size,
            total_block_size(old_payload),
            total_block_size(b->size)
        );
        return ptr;
    }

    // Try to grow in place by checking if the next block is free and has enough space to merge.
    if (b->next != NULL && is_valid_block(b->next) && b->next->free) {
        block_meta_t *next = b->next;
        size_t merged_size = b->size + META_SIZE + next->size;
        if (merged_size >= size) {
            size_t old_payload = b->size;
            b->size = merged_size;
            b->next = next->next;
            if (next->next != NULL) { next->next->prev = b; }

            split_block(b, size);
            debug_log(
                "realloc: grew in place block=%p requested=%zu old_payload=%zu new_payload=%zu old_total=%zu new_total=%zu",
                (void *)b,
                size,
                old_payload,
                b->size,
                total_block_size(old_payload),
                total_block_size(b->size)
            );
            return ptr;
        }
    }

    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) {
        debug_log("realloc: malloc failed for new size %zu", size);
        return NULL;
    }

    memcpy(new_ptr, ptr, b->size);
    my_free(ptr);

    block_meta_t *new_b = ((block_meta_t *)new_ptr) - 1;
    debug_log(
        "realloc: moved block old_ptr=%p new_ptr=%p requested=%zu old_payload=%zu new_payload=%zu old_total=%zu new_total=%zu",
        ptr,
        new_ptr,
        size,
        b->size,
        new_b->size,
        total_block_size(b->size),
        total_block_size(new_b->size)
    );
    return new_ptr;
}

void my_free(void *ptr) {
    if (ptr == NULL) { return; }

    block_meta_t *b = ((block_meta_t *)ptr) - 1;

    if (!is_valid_block(b)) {
        debug_log("free: invalid pointer %p", ptr);
        return;
    }

    if (b->use_mmap) {
        size_t mapping_size = b->mapping_size;

        if (mapping_size == 0) {
            debug_log("free: invalid mmap block %p", (void *)b);
            return;
        }

        debug_log("free: munmap block=%p size=%zu mapping_size=%zu", (void *)b, b->size, mapping_size);
        munmap((void *)b, mapping_size);
        return;
    }

    if (b->free) {
        debug_log("free: double free or duplicate free on block=%p", (void *)b);
        return;
    }

    b->free = 1;
    coalesce(b);
    debug_log("free: block=%p size=%zu marked free and coalesced if possible", (void *)b, b->size);
}
