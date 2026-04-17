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

typedef int (*test_fn_t)(void);

static int run_test_isolated(test_fn_t fn, const char *name, int *passed, int *failed) {
    pid_t pid;
    int status;

    printf("Running %s...\n", name);
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
        printf("PASS: %s\n\n", name);
        (*passed)++;
        return 1;
    }

    printf("FAIL: %s\n\n", name);
    (*failed)++;
    return 0;
}

static int test_malloc_returns_non_null(void) {
    void *p = my_malloc(32);
    TEST_ASSERT(p != NULL, "malloc(32) returned NULL");
    TEST_ASSERT((((uintptr_t)p) & 7ULL) == 0, "returned pointer is not 8-byte aligned");
    TEST_ASSERT(validate_heap() == 1, "heap validation failed after allocation");
    my_free(p);
    return 1;
}

static int test_malloc_zero_returns_null(void) {
    void *p = my_malloc(0);
    TEST_ASSERT(p == NULL, "my_malloc(0) should return NULL");
    TEST_ASSERT(validate_heap() == 1, "heap validation failed after my_malloc(0)");
    return 1;
}

static int test_write_and_read_memory(void) {
    char *p = (char *)my_malloc(64);
    TEST_ASSERT(p != NULL, "allocation failed");

    memset(p, 0x5A, 64);

    for (int i = 0; i < 64; i++) {
        TEST_ASSERT((unsigned char)p[i] == 0x5A, "payload contents mismatch");
    }

    TEST_ASSERT(validate_heap() == 1, "heap validation failed after write/read");
    my_free(p);
    return 1;
}

static int test_free_and_reuse_block(void) {
    size_t before = block_count();

    void *p1 = my_malloc(80);
    TEST_ASSERT(p1 != NULL, "first allocation failed");
    size_t after_first = block_count();
    TEST_ASSERT(after_first == before + 1, "expected one new block after first allocation");

    my_free(p1);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after my_free");

    void *p2 = my_malloc(23);
    TEST_ASSERT(p2 != NULL, "second allocation failed");
    TEST_ASSERT(p2 == p1, "allocator did not reuse compatible free block");

    size_t after_second = block_count();
    TEST_ASSERT(after_second == after_first, "reused allocation should not create a new block");

    my_free(p2);
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
        memset(ptrs[i], i, (size_t)(16 + i * 8));
    }

    for (int i = 0; i < 5; i++) {
        my_free(ptrs[i]);
    }

    TEST_ASSERT(validate_heap() == 1, "heap invalid after multiple frees");
    return 1;
}

static int test_free_null_is_safe(void) {
    my_free(NULL);
    TEST_ASSERT(validate_heap() == 1, "heap invalid after my_free(NULL)");
    return 1;
}

int main(void) {
    int passed = 0;
    int failed = 0;

    run_test_isolated(test_malloc_returns_non_null, "malloc returns non null", &passed, &failed);
    run_test_isolated(test_malloc_zero_returns_null, "malloc zero returns null", &passed, &failed);
    run_test_isolated(test_write_and_read_memory, "writee and read memory", &passed, &failed);
    run_test_isolated(test_free_and_reuse_block, "free and reuse block", &passed, &failed);
    run_test_isolated(test_multiple_allocations, "multiple allocations", &passed, &failed);
    run_test_isolated(test_free_null_is_safe, "free null is safe", &passed, &failed);

    printf("Summary: passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
