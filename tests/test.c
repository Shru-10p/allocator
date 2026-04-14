#include "../src/mini_alloc.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s\n", msg); \
            return 0; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        printf("Running %s...\n", #fn); \
        if (fn()) { \
            printf("PASS: %s\n\n", #fn); \
            passed++; \
        } else { \
            printf("FAIL: %s\n\n", #fn); \
            failed++; \
        } \
    } while (0)

static int test_malloc_returns_non_null(void) {
    void *p = mm_malloc(32);
    TEST_ASSERT(p != NULL, "mm_malloc(32) returned NULL");
    TEST_ASSERT((((uintptr_t)p) & 7ULL) == 0, "returned pointer is not 8-byte aligned");
    TEST_ASSERT(mm_validate_heap() == 1, "heap validation failed after allocation");
    mm_free(p);
    return 1;
}

static int test_malloc_zero_returns_null(void) {
    void *p = mm_malloc(0);
    TEST_ASSERT(p == NULL, "mm_malloc(0) should return NULL");
    TEST_ASSERT(mm_validate_heap() == 1, "heap validation failed after mm_malloc(0)");
    return 1;
}

static int test_write_and_read_memory(void) {
    char *p = (char *)mm_malloc(64);
    TEST_ASSERT(p != NULL, "allocation failed");

    memset(p, 0x5A, 64);

    for (int i = 0; i < 64; i++) {
        TEST_ASSERT((unsigned char)p[i] == 0x5A, "payload contents mismatch");
    }

    TEST_ASSERT(mm_validate_heap() == 1, "heap validation failed after write/read");
    mm_free(p);
    return 1;
}

static int test_free_and_reuse_block(void) {
    size_t before = mm_block_count();

    void *p1 = mm_malloc(80);
    TEST_ASSERT(p1 != NULL, "first allocation failed");
    size_t after_first = mm_block_count();
    TEST_ASSERT(after_first == before + 1, "expected one new block after first allocation");

    mm_free(p1);
    TEST_ASSERT(mm_validate_heap() == 1, "heap invalid after free");

    /*
     * This allocator is first-fit and tests share global allocator state.
     * Ask for >64 so earlier freed blocks from prior tests do not match,
     * making p1 the first compatible free block.
     */
    void *p2 = mm_malloc(72);
    TEST_ASSERT(p2 != NULL, "second allocation failed");
    TEST_ASSERT(p2 == p1, "allocator did not reuse compatible free block");

    size_t after_second = mm_block_count();
    TEST_ASSERT(after_second == after_first, "reused allocation should not create a new block");

    mm_free(p2);
    return 1;
}

static int test_multiple_allocations(void) {
    void *ptrs[5];

    for (int i = 0; i < 5; i++) {
        ptrs[i] = mm_malloc((size_t)(16 + i * 8));
        TEST_ASSERT(ptrs[i] != NULL, "multiple allocation failed");
        TEST_ASSERT(mm_validate_heap() == 1, "heap invalid during multiple allocations");
    }

    for (int i = 0; i < 5; i++) {
        memset(ptrs[i], i, (size_t)(16 + i * 8));
    }

    for (int i = 0; i < 5; i++) {
        mm_free(ptrs[i]);
    }

    TEST_ASSERT(mm_validate_heap() == 1, "heap invalid after multiple frees");
    return 1;
}

static int test_free_null_is_safe(void) {
    mm_free(NULL);
    TEST_ASSERT(mm_validate_heap() == 1, "heap invalid after mm_free(NULL)");
    return 1;
}

int main(void) {
    int passed = 0;
    int failed = 0;

    RUN_TEST(test_malloc_returns_non_null);
    RUN_TEST(test_malloc_zero_returns_null);
    RUN_TEST(test_write_and_read_memory);
    RUN_TEST(test_free_and_reuse_block);
    RUN_TEST(test_multiple_allocations);
    RUN_TEST(test_free_null_is_safe);

    printf("Summary: passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
