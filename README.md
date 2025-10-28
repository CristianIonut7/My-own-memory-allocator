# My Own Memory Allocator

A personal implementation (in C) of the standard memory management functions: `malloc`, `calloc`, `realloc`, and `free`.

## Table of Contents

- [Description](#description)  
- [Features](#features)  
- [Project Structure](#project-structure)  
- [How to Compile and Run](#how-to-compile-and-run)  
- [Usage Example](#usage-example) 

---

## Description

## Description

This project implements a **custom dynamic memory allocator** in C — a low-level replacement for standard library functions such as `malloc`, `calloc`, `realloc`, and `free`.  
The goal is to understand and reproduce the internal mechanisms of memory management, including alignment, metadata handling, block splitting, coalescing, and efficient heap usage.

Unlike standard allocators, this implementation **tracks, reuses, and merges** memory blocks dynamically while minimizing system calls. The allocator maintains metadata for each memory block and optimizes performance through smart reuse and preallocation strategies.

### Implementation Overview

An efficient memory allocator must:

1. **Align memory allocations** — All allocated memory blocks are aligned to 8 bytes, ensuring that both `block_meta` structures and payloads are properly aligned for 64-bit systems. This alignment guarantees atomic access and reduces the risk of misaligned memory operations.

2. **Reuse freed blocks** — Each allocated zone begins with a `block_meta` structure:
   ```c
   struct block_meta {
       size_t size;
       int status;
       struct block_meta *prev;
       struct block_meta *next;
   };
   Blocks are reused whenever possible to reduce fragmentation and system overhead.

Split large blocks — If a free block is larger than the requested size, it is split into two smaller blocks. The first part is allocated, and the remaining space becomes a new reusable free block. Splitting avoids wasting memory but is only performed if the remaining size is large enough to hold another valid block.

Coalesce adjacent free blocks — When multiple free blocks are contiguous, they are merged into a single larger block. This reduces external fragmentation and improves the allocator’s ability to satisfy future large allocations.

Find the best fitting block — The allocator searches through all available blocks and selects the one whose size most closely matches the requested memory, following the best-fit strategy. This minimizes the number of future splits and merges.

Heap preallocation - On the first use, the allocator preallocates a larger chunk of memory (e.g., 128 KB). This reduces the number of brk() system calls and improves performance for subsequent allocations by reusing and splitting from this preallocated region.

---

## Features

- `os_malloc(size_t size)` — allocates a memory block of the given size  
- `os_calloc(size_t nmemb, size_t size)` — allocates memory and initializes it to zero  
- `os_realloc(void *ptr, size_t new_size)` — resizes an existing memory block  
- `os_free(void *ptr)` — frees the allocated memory  
- Additional helper functions available in the `utils` folder (alignment operations, data structures, etc.)

---

## Project Structure
├── src/  
│ ├── osmem.c
├── utils/  
│ └── printf.c
| └── printf.h
| └── osmem.h
| └── block_meta.h
└── README.md

- `src/` contains the core allocator implementation.  
- `utils/` includes helper functions for debugging and headers.  

---

## How to Compile and Run

You need a C development environment (e.g., gcc, make).  

```bash
# Clone the repository
git clone https://github.com/CristianIonut7/My-own-memory-allocator.git
cd My-own-memory-allocator

# Compile (adjust the command depending on your structure)
gcc -I src -I utils src/osmem.c utils/... -o my_allocator

# Run the program or test file
./my_allocator

If you are using a Makefile, you can include a simple build rule like:
all:
    gcc -I src -I utils src/osmem.c utils/... -o my_allocator

#include "osmem.h"

int main() {
    int *arr = (int *) my_malloc(10 * sizeof(int));
    if (!arr) {
        // allocation error
        return 1;
    }

    // use the allocated memory
    for (int i = 0; i < 10; i++) {
        arr[i] = i * 2;
    }

    // resize the array
    arr = (int *) my_realloc(arr, 20 * sizeof(int));

    // free the memory
    my_free(arr);

    return 0;
}


