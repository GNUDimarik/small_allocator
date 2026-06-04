# Memory Allocator

A lightweight dynamic memory allocator designed for operating systems, embedded systems, bootloaders, kernels, and other low-level environments where a dependency on a standard C library allocator is undesirable.

The allocator is based on **Donald Knuth's boundary tag method** and augmented with **256 segregated free-list bins** for fast allocation.

## Features

* Boundary-tag memory management
* Constant-time block coalescing (`O(1)`)
* 256 segregated free-list bins
* Constant-time bin access (`O(1)`)
* Doubly-linked free lists
* First-fit allocation strategy inside bins
* Sorted huge-block bin for reduced fragmentation
* User-supplied memory region
* No dependency on libc allocators
* No dependency on floating-point operations
* Suitable for kernel-space memory management
* Suitable for OS development and embedded environments

---

# Design Overview

The allocator manages memory using the **boundary tag algorithm** described by Donald Knuth.

Each memory block contains:

* Header
* User area
* Footer

Both the header and footer store identical metadata:

* Block size
* Allocation state

This allows the allocator to locate both neighboring blocks in constant time.

## Block Layout

Allocated block:

```text
+-----------+--------------------------------------+-----------+
| Header    |             User Payload             | Footer    |
+-----------+--------------------------------------+-----------+
```

Header/Footer format:

```text
+-------------------------------------------------+
| Size (all bits except LSB) | Allocated Flag     |
+-------------------------------------------------+
```

The least significant bit stores the allocation state:

```text
0 = free
1 = allocated
```

---

# Boundary Tags

Because both ends of the block contain identical metadata, neighboring blocks can be located without maintaining an explicit list of all blocks.

Finding the next block:

```text
next = current + current_size + overhead
```

Finding the previous block:

```text
previous_size = footer_before_current->size
previous      = current - previous_size - overhead
```

Memory layout:

```text
+--------+-----------+--------+
| BlockA |  BlockB   | BlockC |
+--------+-----------+--------+
```

The allocator can move in either direction by reading only the boundary tags.

---

# Block Coalescing

Free block merging occurs during `mem_free()`.

When a block is released, the allocator checks both adjacent blocks.

```text
Before:

+--------+--------+--------+
| Free   | Used   | Free   |
+--------+--------+--------+

           free()
             |
             v

After:

+--------------------------+
|        One Free Block    |
+--------------------------+
```

The merge operation runs in constant time because neighboring blocks are found directly through boundary tags.

Implementation flow:

```cpp
static void *mem_block_erase_merge(void *block)
{
    auto current = mem_block_prev(block);

    if (mem_block_is_free(current)) {
        bin_erase(current);
    }

    current = mem_block_next(block);

    if (mem_block_is_free(current)) {
        bin_erase(current);
    }

    return mem_block_merge(block);
}
```

Adjacent free blocks are first removed from their bins and then merged into a single larger block.

The resulting block is inserted back into the appropriate bin.

---

# Free Block Lists

Free blocks are stored inside segregated bins.

A free block overlays the beginning of its user area with a doubly-linked list node:

```cpp
struct ListHead
{
    ListHead *next;
    ListHead *prev;
};
```

Free block layout:

```text
+----------+------------+------------+------------------+----------+
| Header   | prev ptr   | next ptr   | remaining space  | Footer   |
+----------+------------+------------+------------------+----------+
```

Allocated block layout:

```text
+----------+--------------------------------------+----------+
| Header   |             User Payload             | Footer   |
+----------+--------------------------------------+----------+
```

This approach avoids allocating separate metadata structures and keeps allocator overhead low.

---

# Segregated Bins

The allocator maintains an array of 256 bins:

```cpp
ListHead *gBinList[256];
```

The array is used intentionally because array indexing provides constant-time access:

```text
Bin lookup complexity = O(1)
```

Given a block size:

```cpp
index = mem_block_size(block);
```

Small blocks are mapped directly to a bin whose index equals the block size.

Example:

```text
Size 16 bytes  -> bin[16]
Size 32 bytes  -> bin[32]
Size 64 bytes  -> bin[64]
Size 128 bytes -> bin[128]
```

This allows immediate access to the corresponding free list without traversing unrelated size classes.

---

# Huge Block Bin

Blocks larger than or equal to 256 bytes are stored in a dedicated huge-block bin.

```text
size >= 256 -> bin[255]
```

Insertion logic:

```cpp
template<typename _Node>
_Node *bin_insert(_Node *block)
{
    size_t index = mem_block_size(block);

    if (index < 255) {
        gBinList[index] = free_list_prepend(gBinList[index], block);
    }
    else {
        gBinList[255] =
            free_list_insert_sorted_by_size(
                gBinList[255],
                block);
    }

    return block;
}
```

Unlike the smaller bins, the huge bin is maintained in ascending size order:

```text
bin[255]

[256] -> [272] -> [320] -> [512] -> [4096] -> ...
```

Allocation request:

```text
Request: 300 bytes
```

Selection:

```text
[256] -> too small
[272] -> too small
[320] -> selected
```

This approximates a best-fit strategy and helps reduce external fragmentation.

---

# Allocation

Allocation flow:

```text
Request
   |
   v
Select bin
   |
   v
Find suitable free block
   |
   v
Remove block from bin
   |
   v
Split if necessary
   |
   v
Return pointer
```

If a block is larger than required, it is split:

```text
Before:

+-----------------------------+
|         Free Block          |
+-----------------------------+

After:

+------------+---------------+
| Allocated  |     Free      |
+------------+---------------+
```

The remainder is immediately inserted into the appropriate bin.

---

# Initialization

The allocator operates entirely inside a caller-provided memory region.

Initialization:

```cpp
int mem_initialize(void *base, size_t size);
```

The heap layout is:

```text
+------------+------------------------------------------+------------+
| Prologue   |              Main Heap                   | Epilogue   |
+------------+------------------------------------------+------------+
```

The prologue and epilogue are special allocated blocks used as sentinels.

They eliminate boundary checks during coalescing.

Initial state:

```text
+------------+------------------------------+------------+
| Prologue   |      One Large Free Block    | Epilogue   |
+------------+------------------------------+------------+
```

The free block is inserted into the corresponding bin during initialization.

---

# Bin Storage

Depending on configuration, bins can be stored either:

1. Inside allocator-managed memory

```cpp
#define BINS_ARE_IN_HEAP
```

or

2. In static/global memory

```cpp
ListHead *gBinList[256];
```

This makes the allocator flexible enough for kernels, bootloaders, embedded firmware, and user-space applications.

---

# Reallocation

Reallocation is implemented as:

1. Allocate a new block
2. Copy existing data
3. Free old block

```cpp
void *mem_realloc(void *p, size_t new_sz)
{
    if (!p) {
        return mem_malloc(new_sz);
    }

    auto block = mem_malloc(new_sz);

    if (block) {
        memmove(
            block,
            p,
            min(new_sz, mem_block_size(p))
        );

        mem_free(p);
    }

    return block;
}
```

---

# Diagnostics

The allocator provides integrity checking functions.

Validate a single block:

```cpp
bool mem_check_block(void *p);
```

Validate the entire heap:

```cpp
bool mem_check(bool verbose);
```

Validation verifies:

* Pointer alignment
* Header/footer consistency
* Heap traversal correctness

Boundary-tag validation:

```text
Header == Footer
```

Any mismatch indicates memory corruption.

---

# Complexity

| Operation                  | Complexity |
| -------------------------- | ---------- |
| Bin lookup                 | O(1)       |
| Insert into small bin      | O(1)       |
| Remove from bin            | O(1)       |
| Neighbor discovery         | O(1)       |
| Coalescing                 | O(1)       |
| Allocation from huge bin   | O(n)       |
| Allocation from small bins | O(1)       |

---

# Intended Use Cases

* Hobby operating systems
* Production kernels
* Bootloaders
* Embedded systems
* Bare-metal applications
* Hypervisors

## SAST Tools

[PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.
* Educational projects
* Low-level runtime environments

The allocator prioritizes simplicity, deterministic behavior, low metadata overhead, and ease of integration into operating-system development projects.
