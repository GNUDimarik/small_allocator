/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Dmitry Adzhiev <dmitry.adjiev@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef MEMORY_H
#define MEMORY_H

/* stddef.h Should be provided by an empty compiler if you built it for osdev */
#include <stddef.h>
#include <stdint.h>

#if defined(__OSDEV_HAVE_ERRNO_H__)
#include <errno.h>
#else
#   define EINVAL 22
#endif

#if defined(__OSDEV_DUX_LIBSTDC__)
#include <utils.h>
#else
#   ifndef min
#       define min(a, b)             \
        ({                           \
            __typeof__ (a) _a = (a); \
            __typeof__ (b) _b = (b); \
            _a < _b ? _a : _b;       \
        })
#   endif /* ifndef min */

#   ifndef max
#       define max(a, b)             \
        ({                           \
            __typeof__ (a) _a = (a); \
            __typeof__ (b) _b = (b); \
            _a > _b ? _a : _b;       \
        })
#   endif /* ifndef min */
#endif

int mem_initialize(void *base, size_t size);
void mem_unuinitialize();
void *mem_malloc(size_t size);
void *mem_malloc_aligned(size_t size, size_t alignment);
void *mem_calloc(size_t num, size_t size);
void *mem_realloc(void *p, size_t new_sz);
void mem_free(void *ptr);
[[maybe_unused]] void dump_mem();
[[maybe_unused]] void dump_bins();
[[maybe_unused]] bool mem_block_check(void *p);
[[maybe_unused]] bool mem_check(bool verbose = false);

#endif //MEMORY_H
