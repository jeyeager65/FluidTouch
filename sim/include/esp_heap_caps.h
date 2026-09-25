// Desktop stub for ESP-IDF heap_caps_* (PSRAM allocation) - plain malloc.
#pragma once

#include <stddef.h>
#include <stdlib.h>

#define MALLOC_CAP_SPIRAM (1 << 10)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_DMA (1 << 3)
#define MALLOC_CAP_DEFAULT (1 << 12)

static inline void *heap_caps_malloc(size_t size, unsigned int) { return malloc(size); }
static inline void *heap_caps_calloc(size_t n, size_t size, unsigned int) { return calloc(n, size); }
static inline void *heap_caps_realloc(void *p, size_t size, unsigned int) { return realloc(p, size); }
static inline void heap_caps_free(void *p) { free(p); }
static inline size_t heap_caps_get_free_size(unsigned int) { return 4 * 1024 * 1024; }
static inline size_t heap_caps_get_largest_free_block(unsigned int) { return 2 * 1024 * 1024; }
static inline size_t heap_caps_get_total_size(unsigned int) { return 8 * 1024 * 1024; }
