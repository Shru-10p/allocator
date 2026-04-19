# Memory Allocator

The current implementation is a single-threaded, first-fit allocator with:

- 8-byte payload alignment
- inline block metadata (`block_meta_t`) before each payload
- one global doubly-linked block list
- free-block reuse via first-fit search
- split on reuse when remainder can hold `META_SIZE + 8` bytes minimum
- backward + forward coalescing on `my_free`
- basic debug and heap integrity helpers

## Repository Layout

```text
allocator/
├── Makefile
├── README.md
├── examples/
│   └── example.c
├── src/
│   ├── alloc.h
│   ├── alloc.c
│   ├── alloc_debug.c
│   └── internal/
│       └── internal.h
└── tests/
    └── test.c
```

## Public API Status

Implemented:

- `void *my_malloc(size_t size);`
- `void *my_calloc(size_t nmemb, size_t size);`
- `void  my_free(void *ptr);`

Debug/inspection helpers:

- `void   set_debug(int enabled);`
- `void   debug_print_heap(void);`
- `int    validate_heap(void);`
- `size_t block_count(void);`

## Behavior Notes

- `my_malloc(0)`, `my_calloc(0, x)` and `my_calloc(x, 0)` return `NULL`
- `my_malloc` aligns request size to 8 bytes
- `my_calloc` checks multiplication overflow (`nmemb > SIZE_MAX / size`) and zero-initializes memory
- `my_free(NULL)` is a no-op
- invalid or duplicate frees are logged/ignored in debug mode, but not fully hardened

Internally, the allocator keeps one global block list.

```text
+----------------+--------------+    +----------+---------+
| block metadata | user payload |<-->| metadata | payload |<--...
+----------------+--------------+    +----------+---------+
^                ^
|                |
block header     pointer returned to user
```

A request follows this path:

1. Reject `size == 0`
2. Round up to 8-byte alignment
3. Search the global block list for the **first free block** large enough
4. If found, split it when possible, then mark it in-use and return it
5. Otherwise call `sbrk()` to grow the heap and append a new block

## Build And Run

Build tests + example:

```bash
make
```

Run tests:

```bash
make test
```

Run example program:

```bash
make example
```

## Debugging

Enable debug logging in your driver or tests:

```c
set_debug(1);
```

Then call `debug_print_heap()` and/or `validate_heap()` as needed.

## Current Limits

- single-threaded only
- `O(n)` first-fit scan per allocation
- `sbrk()` backend only
- no `mmap` path for large allocations
- `realloc` not implemented yet
- no hard security checks (canaries/guards/quarantine)
