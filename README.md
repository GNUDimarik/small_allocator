# Implicit blocks list with boundary tags Knuth algoritm with sorted free blocks list and bins
Free blocks sorted by size in bin with index 255 (blocks > 256 bytes size)
Size alighment is sizeof(void*) so min block size is 24 byes because it has sizeof(void*) header and footer with the same size and minimum block sie is sizeof(void*)

### A Block Structure (Allocated or Free):

**Header** – total block size (including header and footer) plus a 1‑bit allocated flag (1 = allocated, 0 = free). The flag is stored in the least significant bit (LSB) because the true size is always a multiple of 8 (LSB = 0).

**Footer** – an exact copy of the header. This allows backward traversal and coalescing.

        ┌──────────────────────────────────────┐
        │  HEADER (8 bytes)                    │
        │  [ total size | 1-bit tag ]          │
        ├──────────────────────────────────────┤
        │  PAYLOAD (user data)                 │
        │  at least sizeof(void*) = 8 bytes    │
        │  (8‑byte aligned)                    │
        ├──────────────────────────────────────┤
        │  FOOTER (8 bytes)                    │
        │  [ total size | 1-bit tag ]          │
        │  (exact copy of header)              │
        └──────────────────────────────────────┘    

### Bins (Free‑List Array)
The allocator reserves an array of 256 pointers (bins[256]).
Each bin points to the head of a singly linked list of free blocks.

When a block is freed, we reuse its payload area to store the next pointer (since the block is free and the payload is not used by the program).
The minimum payload size (8 bytes) is exactly enough for a pointer on 64‑bit.

        ┌──────────────────────────────────────┐  
        │  HEADER (8 bytes)                    │
        │  [ total size | tag=0 (free) ]       │
        ├──────────────────────────────────────┤
        │  PAYLOAD (used as list node)         │
        │  block* next                         │
        ├──────────────────────────────────────┤
        │  FOOTER (8 bytes)                    │
        │  [ total size | tag=0 (free) ]       │
        └──────────────────────────────────────┘

### Bin Indexing Rules

_________________________________________________________________________________________________________________________________
| Block size (true size)      | Bin index        | Sorting order                                                                |
_________________________________________________________________________________________________________________________________
| < 256 bytes                 | size (exact)     | Not sorted                                                                   |
|                             |                  | → makes coalescing faster when merging adjacent free blocks                  |
_________________________________________________________________________________________________________________________________
| >= 256 bytes                | 255              | Sorted by size (descending, largest first)                                   |
|                             |                  | → allows best‑fit allocation without scanning many blocks                    |
_________________________________________________________________________________________________________________________________

Additional notes:
- Small blocks (size < 256) → each exact size has its own dedicated bin (e.g., size 24 → bin[24], size 32 → bin[32]).
- Large blocks (size >= 256) → all go into the same bin (index 255) and are kept sorted by size (largest first).
- At startup, only one huge free block exists (size >= 256) and resides in bin[255].
- When a block is freed and merged, it is inserted into the appropriate bin according to its new total size.

### Allocation Algorithm (Priority Order)
First, search the main free list.
If no suitable block is found there, then search the bins (the array of 256 free‑list heads).

### Why this order?
Bins are filled only during mem_free when coalescing with adjacent free blocks is not possible (or after merging, the resulting block is inserted into a bin).

### At allocator initialization
All bins are empty.
There is only one large free block in the main free list (the entire heap)
Thus, the first allocations always come from scanning the main free list. Only after some memory has been freed and placed into bins 
(because coalescing failed or created a block that fits a bin) will bins contain candidates for future allocations.






