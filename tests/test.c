#include "../src/alloc.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s\n", msg); \
            return 0; \
        } \
    } while (0)

typedef int (*test_fn_t)(void);

static int fill_and_verify(char *ptr, size_t size, unsigned char value) {
    memset(ptr, value, size);

    for (size_t i = 0; i < size; i++) {
        TEST_ASSERT((unsigned char)ptr[i] == value, "payload contents mismatch");
    }

    return 1;
}

// Runs a test function in a separate process to isolate heap state and prevent crashes from affecting other tests.
static int run_test_isolated(test_fn_t fn, const char *name, int *passed, int *failed) {
    pid_t pid;
    int status;

    printf("Running test [%s]\n", name);
    fflush(NULL);

    pid = fork();
    if (pid < 0) {
        printf("FAIL: %s (fork failed)\n\n", name);
        (*failed)++;
        return 0;
    }

    if (pid == 0) {
        int ok = fn();
        _exit(ok ? 0 : 1);
    }

    while (waitpid(pid, &status, 0) < 0) {
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("PASS\n\n");
        (*passed)++;
        return 1;
    }

    printf("\n\n");
    (*failed)++;
    return 0;
}

static int test_basic_allocation_and_null_ops(void) {
    void *zero = my_malloc(0);

    TEST_ASSERT(zero == NULL, "my_malloc(0) should return NULL");
    TEST_ASSERT(validate_heap() == 1, "heap validation failed after my_malloc(0)");

    my_free(NULL);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after my_free(NULL)");

    void *p = my_malloc(32);
    TEST_ASSERT(p != NULL, "malloc(32) returned NULL");
    TEST_ASSERT((((uintptr_t)p) & 7ULL) == 0, "returned pointer is not 8-byte aligned");
    TEST_ASSERT(fill_and_verify((char *)p, 32, 0x5A), "payload contents mismatch");
    TEST_ASSERT(validate_heap() == 1, "heap validation failed after allocation");

    void *q = my_malloc(3);
    TEST_ASSERT(q != NULL, "malloc(3) returned NULL");
    TEST_ASSERT((((uintptr_t)q) & 7ULL) == 0, "malloc(3) returned non-aligned pointer");
    TEST_ASSERT(fill_and_verify((char *)q, 3, 0x2A), "payload contents mismatch for odd-size allocation");
    TEST_ASSERT(validate_heap() == 1, "heap validation failed after odd-size allocation");

    my_free(q);
    my_free(p);
    return 1;
}

static int test_split_behavior(void) {
    size_t baseline = block_count();

    void *large = my_malloc(128);
    TEST_ASSERT(large != NULL, "initial large allocation failed");
    TEST_ASSERT(block_count() == baseline + 1, "expected one block after large allocation");

    my_free(large);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after freeing large block");

    void *split = my_malloc(32);
    TEST_ASSERT(split != NULL, "split allocation failed");
    TEST_ASSERT(split == large, "expected allocator to reuse front of freed block");
    TEST_ASSERT(block_count() == baseline + 2, "expected split to create a remainder block");
    TEST_ASSERT(validate_heap() == 1, "heap invalid after splitting free block");

    my_free(split);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after freeing split block");

    baseline = block_count();

    void *small = my_malloc(64);
    TEST_ASSERT(small != NULL, "medium allocation failed");
    TEST_ASSERT(block_count() == baseline + 1, "expected 64-byte allocation to split the free block");
    my_free(small);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after freeing medium block");
    void *no_split = my_malloc(56);
    TEST_ASSERT(no_split != NULL, "tiny remainder allocation failed");
    TEST_ASSERT(no_split == small, "expected allocator to reuse the coalesced block front");
    TEST_ASSERT(block_count() == baseline + 1, "tiny remainder should not be split into a new block");
    TEST_ASSERT(validate_heap() == 1, "heap invalid after no-split allocation");

    my_free(no_split);
    my_free(small);
    return 1;
}

static int test_coalescing_behavior(void) {
    size_t baseline = block_count();

    void *a = my_malloc(16);
    void *b = my_malloc(40);
    TEST_ASSERT(a != NULL && b != NULL, "failed to allocate two adjacent blocks");
    TEST_ASSERT(block_count() == baseline + 2, "expected two blocks in short coalescing setup");

    my_free(b);
    my_free(a);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after two-block coalescing");
    TEST_ASSERT(block_count() == baseline + 1, "expected adjacent frees to coalesce into one block");

    baseline = block_count();

    void *c = my_malloc(128);
    void *d = my_malloc(128);
    void *e = my_malloc(128);
    void *f = my_malloc(128);
    void *g = my_malloc(128);
    TEST_ASSERT(c != NULL && d != NULL && e != NULL && f != NULL && g != NULL,
                "failed to allocate five adjacent blocks");
    TEST_ASSERT(block_count() == baseline + 5, "expected five blocks in chain");
    my_free(c); // this coalesces with blocks a nd b from before
    my_free(d);
    my_free(g);
    my_free(f);
    TEST_ASSERT(validate_heap() == 1, "heap invalid before center free in long-chain test");
    TEST_ASSERT(block_count() == baseline + 2,
                "expected edge frees to form two coalesced regions plus center block");

    my_free(e);

    TEST_ASSERT(validate_heap() == 1, "heap invalid after center free long-chain coalescing");
    TEST_ASSERT(block_count() == 1, "expected all five adjacent blocks to coalesce into one block");

    return 1;
}

static int test_multiple_allocations(void) {
    void *ptrs[5];

    for (int i = 0; i < 5; i++) {
        ptrs[i] = my_malloc((size_t)(16 + i * 8));
        TEST_ASSERT(ptrs[i] != NULL, "multiple allocation failed");
        TEST_ASSERT(validate_heap() == 1, "heap invalid during multiple allocations");
    }

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(fill_and_verify((char *)ptrs[i], (size_t)(16 + i * 8), (unsigned char)i),
                "payload contents mismatch");
    }

    for (int i = 0; i < 5; i++) {
        my_free(ptrs[i]);
    }

    TEST_ASSERT(validate_heap() == 1, "heap invalid after multiple frees");
    return 1;
}

static int test_calloc(void) {
    size_t baseline = block_count();

    void *c = my_calloc(4, 15);
    TEST_ASSERT(c != NULL, "calloc failed");
    TEST_ASSERT(block_count() == baseline + 1, "expected one block after calloc");
    for (size_t i = 0; i < 64; i++) {
        TEST_ASSERT(((unsigned char *)c)[i] == 0, "calloc did not zero memory");
    }

    my_free(c);

    void *overflow = my_calloc(SIZE_MAX, 2);
    TEST_ASSERT(overflow == NULL, "calloc should return NULL on overflow");
    my_free(overflow);
    return 1;
}

static int test_realloc_behavior(void) {
    // realloc(NULL, n) and realloc(p, 0) semantics
    void *p = my_realloc(NULL, 24);
    TEST_ASSERT(p != NULL, "realloc(NULL, n) should behave like malloc(n)");
    TEST_ASSERT((((uintptr_t)p) & 7ULL) == 0, "realloc(NULL, n) returned non-aligned pointer");
    TEST_ASSERT(validate_heap() == 1, "heap invalid after realloc(NULL, n)");

    void *ret = my_realloc(p, 0);
    TEST_ASSERT(ret == NULL, "realloc(p, 0) should return NULL");
    TEST_ASSERT(validate_heap() == 1, "heap invalid after realloc(p, 0)");

    // in-place growth with payload preservation
    size_t baseline = block_count();

    unsigned char pattern[32];
    for (size_t i = 0; i < sizeof(pattern); i++) {
        pattern[i] = (unsigned char)(0xA0 + (unsigned char)i);
    }

    char *a = (char *)my_malloc(32);
    char *b = (char *)my_malloc(64);
    TEST_ASSERT(a != NULL && b != NULL, "failed setup allocations for realloc in-place growth");
    TEST_ASSERT(block_count() == baseline + 2, "expected two blocks in realloc in-place growth setup");

    memcpy(a, pattern, sizeof(pattern));
    my_free(b);
    TEST_ASSERT(validate_heap() == 1, "heap invalid before realloc in-place growth");

    char *grown = (char *)my_realloc(a, 80);
    TEST_ASSERT(grown == a, "realloc should grow in place by consuming next free block");
    TEST_ASSERT(block_count() == baseline + 2, "expected in-place growth to split and keep two blocks");
    TEST_ASSERT(memcmp(grown, pattern, sizeof(pattern)) == 0,
                "realloc in-place growth did not preserve old payload contents");
    TEST_ASSERT(validate_heap() == 1, "heap invalid after realloc in-place growth");

    my_free(grown);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after freeing grown block");

    // shrink-in-place with payload preservation
    baseline = block_count();

    char *q = (char *)my_malloc(128);
    TEST_ASSERT(q != NULL, "initial allocation failed for realloc shrink test");
    TEST_ASSERT(block_count() == baseline + 1, "expected one block after initial allocation");

    for (size_t i = 0; i < 128; i++) {
        q[i] = (char)(i & 0x7F);
    }

    char *shrunk = (char *)my_realloc(q, 24);
    TEST_ASSERT(shrunk == q, "realloc shrink should keep pointer when block is already large enough");
    TEST_ASSERT(block_count() == baseline + 2, "expected shrink to split off a free remainder block");
    for (size_t i = 0; i < 24; i++) {
        TEST_ASSERT(shrunk[i] == (char)(i & 0x7F), "realloc shrink did not preserve prefix payload");
    }
    TEST_ASSERT(validate_heap() == 1, "heap invalid after realloc shrink");

    my_free(shrunk);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after freeing shrunk block");
    return 1;
}

static int test_invalid_free_pointers(void) {
    size_t baseline = block_count();

    char *p = (char *)my_malloc(64);
    char *q = (char *)my_malloc(16);
    TEST_ASSERT(p != NULL && q != NULL, "setup allocations failed for invalid-free tests");
    TEST_ASSERT(block_count() == baseline + 2, "expected two blocks in invalid-free setup");

    memset(p, 0x7B, 64);

    // Interior pointers are invalid for free; allocator should reject without corrupting heap.
    my_free(p + 8);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after interior-pointer free attempt");
    TEST_ASSERT(block_count() == baseline + 2, "block count changed after interior-pointer free attempt");
    for (size_t i = 0; i < 64; i++) {
        TEST_ASSERT((unsigned char)p[i] == 0x7B, "payload changed after interior-pointer free attempt");
    }

    // Foreign pointer (stack memory) should also be ignored safely.
    int stack_value = 42;
    my_free(&stack_value);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after stack-pointer free attempt");
    TEST_ASSERT(block_count() == baseline + 2, "block count changed after stack-pointer free attempt");

    my_free(q);
    my_free(p);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after cleanup in invalid-free test");
    return 1;
}

static int test_large_allocation_uses_mmap(void) {
    const size_t large_size = 1ULL << 20; // safely above mmap threshold
    size_t baseline = block_count();

    char *small = (char *)my_malloc(64);
    TEST_ASSERT(small != NULL, "small allocation failed in mmap test setup");
    TEST_ASSERT(block_count() == baseline + 1, "small allocation should appear in heap list");
    TEST_ASSERT(validate_heap() == 1, "heap invalid after small allocation in mmap test");

    char *large = (char *)my_malloc(large_size);
    TEST_ASSERT(large != NULL, "large allocation failed");
    TEST_ASSERT((((uintptr_t)large) & 7ULL) == 0, "large allocation returned non-aligned pointer");
    TEST_ASSERT(block_count() == baseline + 1,
                "large mmap allocation should not be inserted in the sbrk heap list");
    TEST_ASSERT(validate_heap() == 1, "heap invalid after large mmap allocation");

    for (size_t i = 0; i < 256; i++) {
        large[i] = (char)(i & 0x7F);
    }

    char *grown = (char *)my_realloc(large, large_size + 8192);
    TEST_ASSERT(grown != NULL, "realloc on mmap-backed block failed");
    for (size_t i = 0; i < 256; i++) {
        TEST_ASSERT(grown[i] == (char)(i & 0x7F), "realloc on mmap-backed block did not preserve payload");
    }
    TEST_ASSERT(block_count() == baseline + 1,
                "mmap realloc should not modify the sbrk heap list");
    TEST_ASSERT(validate_heap() == 1, "heap invalid after realloc on mmap-backed block");

    my_free(grown);
    TEST_ASSERT(block_count() == baseline + 1,
                "freeing mmap-backed block should not change sbrk heap block count");
    TEST_ASSERT(validate_heap() == 1, "heap invalid after freeing mmap-backed block");

    my_free(small);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after mmap test cleanup");
    return 1;
}

int main(int argc, char *argv[]) {
    int passed = 0;
    int failed = 0;
    set_debug((argc > 1) && strcmp(argv[1], "--debug") == 0);

    run_test_isolated(test_basic_allocation_and_null_ops, "basic allocation and null ops", &passed, &failed);
    run_test_isolated(test_split_behavior, "split behavior", &passed, &failed);
    run_test_isolated(test_coalescing_behavior, "coalescing behavior", &passed, &failed);
    run_test_isolated(test_multiple_allocations, "multiple allocations", &passed, &failed);
    run_test_isolated(test_calloc, "calloc behavior", &passed, &failed);
    run_test_isolated(test_realloc_behavior, "realloc behavior", &passed, &failed);
    run_test_isolated(test_invalid_free_pointers, "invalid free pointers", &passed, &failed);
    run_test_isolated(test_large_allocation_uses_mmap, "large allocation (mmap)", &passed, &failed);

    printf("Summary: passed=%d failed=%d\n", passed, failed);
    return !(!failed);
}
