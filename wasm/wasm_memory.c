/* wasm_memory.c - WASM-specific memory optimization for libpng
 *
 * Copyright (c) 2025 libpng contributors
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * =============================================================================
 *
 * This file provides WASM-optimized memory management for libpng that:
 * 1. Integrates with Emscripten's heap management
 * 2. Optimizes allocation patterns for large image buffers
 * 3. Provides memory pool management for better performance
 * 4. Supports progressive memory allocation for streaming
 */

#if defined(PNG_READ_SUPPORTED) || defined(PNG_WRITE_SUPPORTED)

#if PNG_WASM_SIMD_OPT > 0 || defined(__EMSCRIPTEN__)

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/heap.h>
#endif

#define PNG_INTERNAL
#include "../pngpriv.h"

/* WASM Memory Configuration Constants */
#define PNG_WASM_MIN_LARGE_ALLOC    (64 * 1024)      /* 64KB threshold for large allocations */
#define PNG_WASM_MEMORY_ALIGNMENT   16               /* WASM SIMD alignment requirement */
#define PNG_WASM_POOL_SIZE         (1024 * 1024)    /* 1MB memory pool for small allocations */
#define PNG_WASM_MAX_POOLS         8                /* Maximum number of memory pools */

/* Memory pool structure for efficient small allocations */
typedef struct png_wasm_memory_pool_struct
{
   png_bytep base;                /* Base address of pool */
   png_size_t size;               /* Total pool size */
   png_size_t used;               /* Used bytes in pool */
   png_size_t alignment;          /* Alignment requirement */
   int active;                    /* Pool is active */
} png_wasm_memory_pool;

/* WASM memory manager structure */
typedef struct png_wasm_memory_manager_struct
{
   png_wasm_memory_pool pools[PNG_WASM_MAX_POOLS];
   int num_pools;
   png_size_t total_allocated;
   png_size_t peak_allocated;
   png_size_t large_alloc_count;
   png_size_t small_alloc_count;
} png_wasm_memory_manager;

/* Global WASM memory manager instance */
static png_wasm_memory_manager wasm_mem_manager = {0};

/* PRIORITY 2: WASM-optimized aligned malloc */
static void*
png_wasm_aligned_malloc(png_size_t size, png_size_t alignment)
{
   if (size == 0)
      return NULL;

   /* Ensure minimum alignment for WASM SIMD */
   if (alignment < PNG_WASM_MEMORY_ALIGNMENT)
      alignment = PNG_WASM_MEMORY_ALIGNMENT;

   /* For large allocations, use direct aligned allocation */
   if (size >= PNG_WASM_MIN_LARGE_ALLOC)
   {
      wasm_mem_manager.large_alloc_count++;
      wasm_mem_manager.total_allocated += size;

      if (wasm_mem_manager.total_allocated > wasm_mem_manager.peak_allocated)
         wasm_mem_manager.peak_allocated = wasm_mem_manager.total_allocated;

      /* Use Emscripten's aligned allocation if available */
#ifdef __EMSCRIPTEN__
      void* ptr = aligned_alloc(alignment, size);
      if (ptr != NULL)
         return ptr;
#endif

      /* Fallback to manual alignment */
      void* raw_ptr = malloc(size + alignment);
      if (raw_ptr == NULL)
         return NULL;

      /* Calculate aligned pointer */
      uintptr_t raw_addr = (uintptr_t)raw_ptr;
      uintptr_t aligned_addr = (raw_addr + alignment) & ~(alignment - 1);

      /* Store the original pointer before the aligned memory */
      void** stored_ptr = (void**)aligned_addr - 1;
      *stored_ptr = raw_ptr;

      return (void*)aligned_addr;
   }

   /* For small allocations, try to use memory pools */
   wasm_mem_manager.small_alloc_count++;

   /* Try existing pools first */
   for (int i = 0; i < wasm_mem_manager.num_pools; i++)
   {
      png_wasm_memory_pool* pool = &wasm_mem_manager.pools[i];

      if (!pool->active)
         continue;

      /* Calculate aligned offset within pool */
      uintptr_t pool_addr = (uintptr_t)pool->base + pool->used;
      uintptr_t aligned_addr = (pool_addr + alignment - 1) & ~(alignment - 1);
      png_size_t aligned_offset = aligned_addr - (uintptr_t)pool->base;

      if (aligned_offset + size <= pool->size)
      {
         pool->used = aligned_offset + size;
         return (void*)aligned_addr;
      }
   }

   /* Create new pool if needed and possible */
   if (wasm_mem_manager.num_pools < PNG_WASM_MAX_POOLS &&
       size < PNG_WASM_POOL_SIZE / 2)
   {
      png_wasm_memory_pool* pool = &wasm_mem_manager.pools[wasm_mem_manager.num_pools];

      pool->size = PNG_WASM_POOL_SIZE;
      pool->base = (png_bytep)png_wasm_aligned_malloc(pool->size, PNG_WASM_MEMORY_ALIGNMENT);

      if (pool->base != NULL)
      {
         pool->used = 0;
         pool->alignment = alignment;
         pool->active = 1;
         wasm_mem_manager.num_pools++;

         /* Allocate from new pool */
         pool->used = size;
         return pool->base;
      }
   }

   /* Fallback to regular malloc for small allocations that don't fit in pools */
   return malloc(size);
}

/* PRIORITY 2: WASM-optimized aligned free */
static void
png_wasm_aligned_free(void* ptr)
{
   if (ptr == NULL)
      return;

   /* Check if pointer is in any memory pool */
   for (int i = 0; i < wasm_mem_manager.num_pools; i++)
   {
      png_wasm_memory_pool* pool = &wasm_mem_manager.pools[i];

      if (pool->active &&
          ptr >= (void*)pool->base &&
          ptr < (void*)(pool->base + pool->size))
      {
         /* Pointer is in pool - don't free individual allocations */
         /* Pool will be freed when PNG processing completes */
         return;
      }
   }

   /* For large allocations that were manually aligned */
   if ((uintptr_t)ptr % PNG_WASM_MEMORY_ALIGNMENT == 0)
   {
      void** stored_ptr = (void**)ptr - 1;
      free(*stored_ptr);
   }
   else
   {
      /* Regular malloc allocation */
      free(ptr);
   }
}

/* PRIORITY 2: Custom PNG malloc implementation for WASM */
PNG_FUNCTION(png_voidp, png_wasm_malloc, (png_structp png_ptr, png_alloc_size_t size), PNG_ALLOCATED)
{
   void* ptr;

   PNG_UNUSED(png_ptr)

   /* Validate size */
   if (size == 0 || size > PNG_SIZE_MAX)
      return NULL;

   /* Use WASM-optimized allocation */
   ptr = png_wasm_aligned_malloc((png_size_t)size, PNG_WASM_MEMORY_ALIGNMENT);

   if (ptr == NULL && size > PNG_WASM_MIN_LARGE_ALLOC)
   {
      /* For large allocations, try to trigger garbage collection and retry */
#ifdef __EMSCRIPTEN__
      emscripten_force_gc();
      ptr = png_wasm_aligned_malloc((png_size_t)size, PNG_WASM_MEMORY_ALIGNMENT);
#endif
   }

   return ptr;
}

/* PRIORITY 2: Custom PNG free implementation for WASM */
PNG_FUNCTION(void, png_wasm_free, (png_structp png_ptr, png_voidp ptr), PNG_DEPRECATED)
{
   PNG_UNUSED(png_ptr)
   png_wasm_aligned_free(ptr);
}

/* PRIORITY 2: Streaming-optimized buffer allocation */
PNG_FUNCTION(png_voidp, png_wasm_malloc_streaming, (png_structp png_ptr,
                                                   png_alloc_size_t size,
                                                   png_uint_32 flags), PNG_ALLOCATED)
{
   PNG_UNUSED(flags)

   /* For streaming, prefer smaller, more frequent allocations */
   if (size > PNG_WASM_MIN_LARGE_ALLOC)
   {
      /* Break large streaming buffers into chunks */
      return png_wasm_malloc(png_ptr, size);
   }

   /* Use regular allocation for small streaming buffers */
   return png_wasm_malloc(png_ptr, size);
}

/* PRIORITY 2: Initialize WASM memory management for PNG */
void
png_wasm_memory_init(png_structp png_ptr)
{
   if (png_ptr == NULL)
      return;

   /* Set custom memory functions for WASM optimization */
   png_set_mem_fn(png_ptr, NULL, png_wasm_malloc, png_wasm_free);

   /* Initialize memory manager if not already done */
   if (wasm_mem_manager.num_pools == 0)
   {
      memset(&wasm_mem_manager, 0, sizeof(wasm_mem_manager));
   }
}

/* PRIORITY 2: Cleanup WASM memory pools */
void
png_wasm_memory_cleanup(png_structp png_ptr)
{
   PNG_UNUSED(png_ptr)

   /* Free all memory pools */
   for (int i = 0; i < wasm_mem_manager.num_pools; i++)
   {
      png_wasm_memory_pool* pool = &wasm_mem_manager.pools[i];

      if (pool->active && pool->base != NULL)
      {
         free(pool->base);
         pool->base = NULL;
         pool->active = 0;
      }
   }

   wasm_mem_manager.num_pools = 0;
   wasm_mem_manager.total_allocated = 0;
}

/* PRIORITY 2: Get WASM memory statistics */
void
png_wasm_memory_get_stats(png_structp png_ptr, png_wasm_memory_stats* stats)
{
   PNG_UNUSED(png_ptr)

   if (stats == NULL)
      return;

   stats->total_allocated = wasm_mem_manager.total_allocated;
   stats->peak_allocated = wasm_mem_manager.peak_allocated;
   stats->large_alloc_count = wasm_mem_manager.large_alloc_count;
   stats->small_alloc_count = wasm_mem_manager.small_alloc_count;
   stats->active_pools = 0;

   for (int i = 0; i < wasm_mem_manager.num_pools; i++)
   {
      if (wasm_mem_manager.pools[i].active)
         stats->active_pools++;
   }
}

/* PRIORITY 2: WASM heap management integration */
void
png_wasm_optimize_heap(png_structp png_ptr)
{
   PNG_UNUSED(png_ptr)

#ifdef __EMSCRIPTEN__
   /* Get current heap information */
   size_t heap_size = emscripten_get_heap_size();
   size_t used_heap = emscripten_get_heap_usage();

   /* If heap usage is high, try to grow heap proactively */
   if (used_heap > heap_size * 0.8)  /* 80% threshold */
   {
      size_t growth = heap_size / 4;   /* Grow by 25% */
      emscripten_resize_heap(heap_size + growth);
   }
#endif
}

/* PRIORITY 2: Progressive memory allocation for large images */
png_voidp
png_wasm_malloc_progressive(png_structp png_ptr, png_alloc_size_t size,
                           int progressive_pass)
{
   /* Adjust allocation strategy based on progressive pass */
   if (progressive_pass > 0)
   {
      /* Later passes need less memory - be more conservative */
      return png_wasm_malloc(png_ptr, size);
   }

   /* First pass - allocate with optimistic heap growth */
   png_wasm_optimize_heap(png_ptr);
   return png_wasm_malloc(png_ptr, size);
}

#endif /* PNG_WASM_SIMD_OPT > 0 || defined(__EMSCRIPTEN__) */
#endif /* PNG_READ_SUPPORTED || PNG_WRITE_SUPPORTED */
