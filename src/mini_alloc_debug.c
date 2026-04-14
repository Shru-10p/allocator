#include "mini_alloc.h"
#include "mini_alloc_internal.h"

#include <stdarg.h>
#include <stdio.h>

static int g_debug = 0;

void mm_set_debug(int enabled) {
    g_debug = enabled ? 1 : 0;
}

void mm_debug_log(const char *fmt, ...) {
    va_list args;

    if (!g_debug) {
        return;
    }

    fprintf(stderr, "[mini_alloc] ");

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}

size_t mm_block_count(void) {
    size_t count = 0;
    block_meta_t *cur = g_base;

    while (cur != NULL) {
        if (!mm_is_valid_block(cur)) {
            return count;
        }
        count++;
        cur = cur->next;
    }

    return count;
}

int mm_validate_heap(void) {
    block_meta_t *cur = g_base;
    block_meta_t *prev = NULL;

    while (cur != NULL) {
        if (!mm_is_valid_block(cur)) {
            mm_debug_log("mm_validate_heap: invalid block at %p", (void *)cur);
            return 0;
        }

        if (cur->prev != prev) {
            mm_debug_log("mm_validate_heap: backward-link mismatch at %p", (void *)cur);
            return 0;
        }

        if (cur->next != NULL && cur->next->prev != cur) {
            mm_debug_log("mm_validate_heap: forward-link mismatch at %p", (void *)cur);
            return 0;
        }

        prev = cur;
        cur = cur->next;
    }

    return 1;
}

void mm_debug_print_heap(void) {
    block_meta_t *cur = g_base;
    size_t idx = 0;

    printf("=== heap dump ===\n");
    while (cur != NULL) {
        if (!mm_is_valid_block(cur)) {
            printf("[%zu] INVALID BLOCK at %p\n", idx, (void *)cur);
            break;
        }

        printf("[%zu] block=%p payload=%p size=%zu free=%u prev=%p next=%p\n",
                idx,
                (void *)cur,
                (void *)(cur + 1),
                cur->size,
                (unsigned)cur->free,
                (void *)cur->prev,
                (void *)cur->next);

        cur = cur->next;
        idx++;
    }
    printf("=================\n");
}
