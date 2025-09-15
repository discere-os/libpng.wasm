/* wasm_progressive.c - WASM-optimized progressive PNG streaming support
 *
 * Copyright (c) 2025 libpng contributors
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * ======================================================
 *
 * This file provides WASM-optimized progressive PNG processing that:
 * 1. Implements efficient streaming decode/encode
 * 2. Minimizes memory footprint during progressive operations
 * 3. Provides JavaScript-friendly callback mechanisms
 * 4. Optimizes for network-based PNG loading scenarios
 */

#ifdef PNG_PROGRESSIVE_READ_SUPPORTED

#if PNG_WASM_SIMD_OPT > 0 || defined(__EMSCRIPTEN__)

#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define PNG_INTERNAL
#include "../pngpriv.h"

/* WASM Progressive Configuration Constants */
#define PNG_WASM_PROGRESSIVE_BUFFER_SIZE    (32 * 1024)  /* 32KB progressive buffer */
#define PNG_WASM_PROGRESSIVE_CHUNK_SIZE     (4 * 1024)   /* 4KB processing chunks */
#define PNG_WASM_MAX_PROGRESSIVE_CALLBACKS  10           /* Maximum callback queue */

/* Progressive callback types for WASM/JavaScript integration */
typedef enum
{
   PNG_WASM_CALLBACK_INFO_READY,      /* PNG info/header available */
   PNG_WASM_CALLBACK_ROW_READY,       /* PNG row data available */
   PNG_WASM_CALLBACK_IMAGE_COMPLETE,  /* PNG decoding complete */
   PNG_WASM_CALLBACK_ERROR,           /* Error occurred */
   PNG_WASM_CALLBACK_PROGRESS         /* Progress update */
} png_wasm_callback_type;

/* Progressive callback structure */
typedef struct png_wasm_progressive_callback_struct
{
   png_wasm_callback_type type;
   void (*callback)(png_structp, png_wasm_callback_type, void*);
   void* user_data;
   int active;
} png_wasm_progressive_callback;

/* WASM progressive context */
typedef struct png_wasm_progressive_context_struct
{
   png_structp png_ptr;
   png_infop info_ptr;

   /* Buffer management */
   png_bytep buffer;
   png_size_t buffer_size;
   png_size_t buffer_used;
   png_size_t total_processed;

   /* Progressive state */
   int header_processed;
   int info_callback_sent;
   png_uint_32 current_row;
   png_uint_32 total_rows;

   /* Callbacks */
   png_wasm_progressive_callback callbacks[PNG_WASM_MAX_PROGRESSIVE_CALLBACKS];
   int callback_count;

   /* Performance tracking */
   png_uint_32 bytes_per_second;
   png_uint_32 processing_start_time;

   /* Memory optimization */
   png_wasm_memory_stats memory_stats;

} png_wasm_progressive_context;

/* PRIORITY 2: Initialize WASM progressive context */
PNG_FUNCTION(png_wasm_progressive_context*, png_wasm_progressive_create,
            (png_structp png_ptr, png_infop info_ptr), PNG_ALLOCATED)
{
   png_wasm_progressive_context* context;

   if (png_ptr == NULL)
      return NULL;

   context = (png_wasm_progressive_context*)png_malloc(png_ptr,
                                                      sizeof(png_wasm_progressive_context));
   if (context == NULL)
      return NULL;

   /* Initialize context */
   memset(context, 0, sizeof(png_wasm_progressive_context));
   context->png_ptr = png_ptr;
   context->info_ptr = info_ptr;

   /* Allocate progressive buffer */
   context->buffer_size = PNG_WASM_PROGRESSIVE_BUFFER_SIZE;
   context->buffer = (png_bytep)png_wasm_malloc_streaming(png_ptr,
                                                         context->buffer_size, 0);

   if (context->buffer == NULL)
   {
      png_free(png_ptr, context);
      return NULL;
   }

   /* Initialize WASM memory management */
   png_wasm_memory_init(png_ptr);

   return context;
}

/* PRIORITY 2: Add progressive callback */
PNG_FUNCTION(int, png_wasm_progressive_add_callback,
            (png_wasm_progressive_context* context,
             png_wasm_callback_type type,
             void (*callback)(png_structp, png_wasm_callback_type, void*),
             void* user_data), PNG_EMPTY)
{
   if (context == NULL || callback == NULL)
      return 0;

   if (context->callback_count >= PNG_WASM_MAX_PROGRESSIVE_CALLBACKS)
      return 0;

   png_wasm_progressive_callback* cb = &context->callbacks[context->callback_count];
   cb->type = type;
   cb->callback = callback;
   cb->user_data = user_data;
   cb->active = 1;

   context->callback_count++;
   return 1;
}

/* PRIORITY 2: Internal callback dispatcher */
static void
png_wasm_progressive_dispatch_callback(png_wasm_progressive_context* context,
                                      png_wasm_callback_type type,
                                      void* data)
{
   for (int i = 0; i < context->callback_count; i++)
   {
      png_wasm_progressive_callback* cb = &context->callbacks[i];

      if (cb->active && (cb->type == type || cb->type == PNG_WASM_CALLBACK_PROGRESS))
      {
         cb->callback(context->png_ptr, type, data);
      }
   }
}

/* PRIORITY 2: Progressive info callback - called when PNG header is available */
static void
png_wasm_progressive_info_callback(png_structp png_ptr, png_infop info_ptr)
{
   png_wasm_progressive_context* context;

   /* Retrieve context from png_ptr */
   context = (png_wasm_progressive_context*)png_get_progressive_ptr(png_ptr);
   if (context == NULL)
      return;

   context->header_processed = 1;
   context->total_rows = png_get_image_height(png_ptr, info_ptr);

   /* Optimize memory allocation based on image dimensions */
   png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
   png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
   png_byte channels = png_get_channels(png_ptr, info_ptr);
   png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

   /* Calculate memory requirements and optimize heap */
   png_size_t row_bytes = width * channels * (bit_depth / 8);
   png_size_t total_image_size = row_bytes * height;

   if (total_image_size > PNG_WASM_MIN_LARGE_ALLOC)
   {
      png_wasm_optimize_heap(png_ptr);
   }

   /* Dispatch info ready callback */
   if (!context->info_callback_sent)
   {
      png_wasm_progressive_dispatch_callback(context, PNG_WASM_CALLBACK_INFO_READY, info_ptr);
      context->info_callback_sent = 1;
   }
}

/* PRIORITY 2: Progressive row callback - called for each decoded row */
static void
png_wasm_progressive_row_callback(png_structp png_ptr, png_bytep new_row,
                                 png_uint_32 row_num, int pass)
{
   png_wasm_progressive_context* context;

   context = (png_wasm_progressive_context*)png_get_progressive_ptr(png_ptr);
   if (context == NULL)
      return;

   context->current_row = row_num;

   /* Create row data structure for callback */
   struct {
      png_bytep row_data;
      png_uint_32 row_number;
      int pass_number;
      float progress;
   } row_info;

   row_info.row_data = new_row;
   row_info.row_number = row_num;
   row_info.pass_number = pass;
   row_info.progress = context->total_rows > 0 ?
                      ((float)row_num / (float)context->total_rows) * 100.0f : 0.0f;

   /* Dispatch row ready callback */
   png_wasm_progressive_dispatch_callback(context, PNG_WASM_CALLBACK_ROW_READY, &row_info);

   /* Update memory statistics */
   png_wasm_memory_get_stats(png_ptr, &context->memory_stats);

   PNG_UNUSED(pass)
}

/* PRIORITY 2: Progressive end callback - called when decoding is complete */
static void
png_wasm_progressive_end_callback(png_structp png_ptr, png_infop info_ptr)
{
   png_wasm_progressive_context* context;

   context = (png_wasm_progressive_context*)png_get_progressive_ptr(png_ptr);
   if (context == NULL)
      return;

   /* Calculate final performance statistics */
#ifdef __EMSCRIPTEN__
   png_uint_32 end_time = (png_uint_32)emscripten_get_now();
   if (context->processing_start_time > 0 && end_time > context->processing_start_time)
   {
      png_uint_32 duration_ms = end_time - context->processing_start_time;
      if (duration_ms > 0)
      {
         context->bytes_per_second = (context->total_processed * 1000) / duration_ms;
      }
   }
#endif

   /* Create completion data structure */
   struct {
      png_infop info_ptr;
      png_uint_32 total_bytes_processed;
      png_uint_32 processing_time_ms;
      png_uint_32 bytes_per_second;
      png_wasm_memory_stats memory_stats;
   } completion_info;

   completion_info.info_ptr = info_ptr;
   completion_info.total_bytes_processed = context->total_processed;
   completion_info.processing_time_ms = 0;
   completion_info.bytes_per_second = context->bytes_per_second;
   completion_info.memory_stats = context->memory_stats;

#ifdef __EMSCRIPTEN__
   if (context->processing_start_time > 0)
   {
      completion_info.processing_time_ms =
         (png_uint_32)emscripten_get_now() - context->processing_start_time;
   }
#endif

   /* Dispatch completion callback */
   png_wasm_progressive_dispatch_callback(context, PNG_WASM_CALLBACK_IMAGE_COMPLETE,
                                         &completion_info);
}

/* PRIORITY 2: Initialize progressive reading with WASM optimizations */
PNG_FUNCTION(int, png_wasm_progressive_read_init,
            (png_wasm_progressive_context* context), PNG_EMPTY)
{
   if (context == NULL || context->png_ptr == NULL)
      return 0;

   /* Set up progressive reading callbacks */
   png_set_progressive_read_fn(context->png_ptr,
                              (png_voidp)context,                      /* progressive_ptr */
                              png_wasm_progressive_info_callback,      /* info_fn */
                              png_wasm_progressive_row_callback,       /* row_fn */
                              png_wasm_progressive_end_callback);      /* end_fn */

   /* Record processing start time */
#ifdef __EMSCRIPTEN__
   context->processing_start_time = (png_uint_32)emscripten_get_now();
#endif

   return 1;
}

/* PRIORITY 2: Process chunk of PNG data progressively */
PNG_FUNCTION(int, png_wasm_progressive_read_chunk,
            (png_wasm_progressive_context* context,
             png_const_bytep data, png_size_t length), PNG_EMPTY)
{
   if (context == NULL || context->png_ptr == NULL || data == NULL || length == 0)
      return 0;

   /* Process data in smaller chunks for better memory management */
   png_size_t remaining = length;
   png_const_bytep current_data = data;

   while (remaining > 0)
   {
      png_size_t chunk_size = (remaining > PNG_WASM_PROGRESSIVE_CHUNK_SIZE) ?
                             PNG_WASM_PROGRESSIVE_CHUNK_SIZE : remaining;

      /* Process chunk */
      png_process_data(context->png_ptr, context->info_ptr,
                      (png_bytep)current_data, chunk_size);

      /* Update counters */
      current_data += chunk_size;
      remaining -= chunk_size;
      context->total_processed += chunk_size;

      /* Allow WASM event loop to run */
#ifdef __EMSCRIPTEN__
      if (remaining > 0)
      {
         emscripten_sleep(0);  /* Yield to browser */
      }
#endif
   }

   return 1;
}

/* PRIORITY 2: Complete progressive reading */
PNG_FUNCTION(int, png_wasm_progressive_read_complete,
            (png_wasm_progressive_context* context), PNG_EMPTY)
{
   if (context == NULL || context->png_ptr == NULL)
      return 0;

   /* Ensure all data has been processed */
   png_process_data_pause(context->png_ptr, 0);

   return 1;
}

/* PRIORITY 2: Cleanup progressive context */
PNG_FUNCTION(void, png_wasm_progressive_destroy,
            (png_wasm_progressive_context* context), PNG_EMPTY)
{
   if (context == NULL)
      return;

   /* Free progressive buffer */
   if (context->buffer != NULL && context->png_ptr != NULL)
   {
      png_free(context->png_ptr, context->buffer);
   }

   /* Cleanup WASM memory management */
   if (context->png_ptr != NULL)
   {
      png_wasm_memory_cleanup(context->png_ptr);
   }

   /* Free context itself */
   if (context->png_ptr != NULL)
   {
      png_free(context->png_ptr, context);
   }
}

/* PRIORITY 2: Get progressive processing statistics */
PNG_FUNCTION(void, png_wasm_progressive_get_stats,
            (png_wasm_progressive_context* context,
             png_wasm_progressive_stats* stats), PNG_EMPTY)
{
   if (context == NULL || stats == NULL)
      return;

   stats->total_bytes_processed = context->total_processed;
   stats->current_row = context->current_row;
   stats->total_rows = context->total_rows;
   stats->bytes_per_second = context->bytes_per_second;
   stats->memory_usage = context->memory_stats;

   if (context->total_rows > 0)
   {
      stats->progress_percent = ((float)context->current_row / (float)context->total_rows) * 100.0f;
   }
   else
   {
      stats->progress_percent = 0.0f;
   }
}

#endif /* PNG_WASM_SIMD_OPT > 0 || defined(__EMSCRIPTEN__) */
#endif /* PNG_PROGRESSIVE_READ_SUPPORTED */
