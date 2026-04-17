# Memory Allocator

A small heap allocator in C.

Right now it is a **single-threaded, first-fit allocator** with:

- 8-byte alignment
- inline block metadata
- one global doubly-linked block list
- free-block reuse with first-fit search
- block splitting on reuse when remainder is large enough

## Repository layout

```text
allocator/
├── Makefile
├── README.md
├── TODO.md
├── NOTES.md
├── gitignore
├── examples/
│   └── example.c
├── src/
│   ├── alloc.h
│   ├── alloc.c
│   ├── alloc_debug.c
│   └── alloc_internal.h
└── tests/
    └── test.c
```

## What it does (so far)

The allocator exposes two public functions:

- `void *my_malloc(size_t size);`
- `void  my_free(void *ptr);`

Internally, it keeps one global block list. Each block has a header immediately before the user payload.

Memory layout of one block:

```text
+-------------------------+----------------------+
| struct block_meta       | user payload bytes   |
+-------------------------+----------------------+
^                         ^
|                         |
block header              pointer returned to user
```

A request follows this path:

1. Reject `size == 0`
2. Round up to 8-byte alignment
3. Search the global block list for the **first free block** large enough
4. If found, split it when possible, then mark it in-use and return it
5. Otherwise call `sbrk()` to grow the heap and append a new block

`my_free()` only marks a block as free. It does **not** coalesce neighboring free blocks yet.

## Build

```bash
make
```

## Run tests

```bash
make test
./tests/test
```

Or in one step:

```bash
make run
```

## Run with debugging enabled

The implementation includes a small debug hook:

```c
set_debug(1);
```

Then rebuild and run tests or your own driver.

## API

### `void *my_malloc(size_t size);`

Allocates `size` bytes from the process heap.

Behavior:

- returns `NULL` if `size == 0`
- returns an 8-byte-aligned pointer on success
- returns `NULL` if `sbrk()` fails

### `void my_free(void *ptr);`

Marks a previously allocated block as free.

Behavior:

- `my_free(NULL)` is a no-op
- it currently does not detect every invalid pointer
- it currently does not coalesce neighboring free blocks

### Debug / inspection helpers

The header also exposes these helpers:

- `void set_debug(int enabled);`
- `void debug_print_heap(void);`
- `int  validate_heap(void);`
- `size_t block_count(void);`

These are included for testing and debugging.

## Design limitations

- single-threaded only
- `O(n)` first-fit search
- no coalescing, so fragmentation can accumulate
- `sbrk()` only
- no large-block `mmap()` path
- no `calloc` / `realloc`
- no security hardening
