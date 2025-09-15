/* wasm_zlib.c - WASM-optimized zlib integration for libpng
 *
 * Copyright (c) 2025 libpng contributors
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * ======================================================================
 *
 * This file provides WASM-optimized zlib integration that:
 * 1. Optimizes compression buffer management for WASM memory constraints
 * 2. Integrates with shared zlib.wasm dependency
 * 3. Provides efficient streaming compression/decompression
 * 4. Minimizes memory allocations during compression
 */

#if defined(PNG_READ_SUPPORTED) || defined(PNG_WRITE_SUPPORTED)

#if PNG_WASM_SIMD_OPT > 0 || defined(__EMSCRIPTEN__)

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define PNG_INTERNAL
#include "../pngpriv.h"

/* WASM zlib Configuration Constants */
#define PNG_WASM_ZLIB_BUFFER_SIZE      (64 * 1024)  /* 64KB zlib buffer */
#define PNG_WASM_ZLIB_CHUNK_SIZE       (16 * 1024)  /* 16KB processing chunks */
#define PNG_WASM_ZLIB_WINDOW_BITS      15          /* Standard window size */
#define PNG_WASM_ZLIB_MEM_LEVEL        8           /* Memory level for WASM */

/* WASM zlib buffer management structure */
typedef struct png_wasm_zlib_buffer_struct
{
   png_bytep input_buffer;       /* Input compression buffer */
   png_bytep output_buffer;      /* Output compression buffer */
   png_size_t input_size;        /* Input buffer size */
   png_size_t output_size;       /* Output buffer size */
   png_size_t input_used;        /* Used bytes in input buffer */
   png_size_t output_used;       /* Used bytes in output buffer */
   z_stream* stream;             /* zlib stream structure */
   int compression_level;        /* Current compression level */
   int initialized;              /* Buffer is initialized */
} png_wasm_zlib_buffer;

/* Global WASM zlib buffer (shared across PNG instances for efficiency) */
static png_wasm_zlib_buffer wasm_zlib_buffer = {0};

/* PRIORITY 2: Initialize WASM zlib buffer management */
static int
png_wasm_zlib_init_buffer(png_structp png_ptr, int compression_level)
{
   if (wasm_zlib_buffer.initialized)
   {
      /* Reuse existing buffer if compression level matches */
      if (wasm_zlib_buffer.compression_level == compression_level)
         return 1;

      /* Otherwise cleanup and reinitialize */
      png_wasm_zlib_cleanup_buffer(png_ptr);
   }

   /* Allocate input buffer */
   wasm_zlib_buffer.input_size = PNG_WASM_ZLIB_BUFFER_SIZE;
   wasm_zlib_buffer.input_buffer = (png_bytep)png_wasm_malloc_streaming(png_ptr,
                                                                        wasm_zlib_buffer.input_size, 0);
   if (wasm_zlib_buffer.input_buffer == NULL)
      return 0;

   /* Allocate output buffer */
   wasm_zlib_buffer.output_size = PNG_WASM_ZLIB_BUFFER_SIZE;
   wasm_zlib_buffer.output_buffer = (png_bytep)png_wasm_malloc_streaming(png_ptr,
                                                                         wasm_zlib_buffer.output_size, 0);
   if (wasm_zlib_buffer.output_buffer == NULL)
   {
      png_free(png_ptr, wasm_zlib_buffer.input_buffer);
      return 0;
   }

   /* Allocate zlib stream structure */
   wasm_zlib_buffer.stream = (z_stream*)png_malloc(png_ptr, sizeof(z_stream));
   if (wasm_zlib_buffer.stream == NULL)
   {
      png_free(png_ptr, wasm_zlib_buffer.input_buffer);
      png_free(png_ptr, wasm_zlib_buffer.output_buffer);
      return 0;
   }

   /* Initialize zlib stream */
   memset(wasm_zlib_buffer.stream, 0, sizeof(z_stream));
   wasm_zlib_buffer.compression_level = compression_level;
   wasm_zlib_buffer.input_used = 0;
   wasm_zlib_buffer.output_used = 0;
   wasm_zlib_buffer.initialized = 1;

   return 1;
}

/* PRIORITY 2: Cleanup WASM zlib buffer management */
PNG_FUNCTION(void, png_wasm_zlib_cleanup_buffer, (png_structp png_ptr), PNG_EMPTY)
{
   if (!wasm_zlib_buffer.initialized)
      return;

   /* Free buffers */
   if (wasm_zlib_buffer.input_buffer != NULL && png_ptr != NULL)
   {
      png_free(png_ptr, wasm_zlib_buffer.input_buffer);
      wasm_zlib_buffer.input_buffer = NULL;
   }

   if (wasm_zlib_buffer.output_buffer != NULL && png_ptr != NULL)
   {
      png_free(png_ptr, wasm_zlib_buffer.output_buffer);
      wasm_zlib_buffer.output_buffer = NULL;
   }

   if (wasm_zlib_buffer.stream != NULL && png_ptr != NULL)
   {
      /* End zlib stream if it was initialized */
      deflateEnd(wasm_zlib_buffer.stream);
      inflateEnd(wasm_zlib_buffer.stream);

      png_free(png_ptr, wasm_zlib_buffer.stream);
      wasm_zlib_buffer.stream = NULL;
   }

   wasm_zlib_buffer.initialized = 0;
}

/* PRIORITY 2: WASM-optimized PNG compression initialization */
PNG_FUNCTION(int, png_wasm_zlib_compress_init,
            (png_structp png_ptr, int level, int method, int window_bits,
             int mem_level, int strategy), PNG_EMPTY)
{
   if (png_ptr == NULL)
      return Z_STREAM_ERROR;

   /* Initialize WASM zlib buffer */
   if (!png_wasm_zlib_init_buffer(png_ptr, level))
      return Z_MEM_ERROR;

   /* Initialize deflate stream with WASM-optimized parameters */
   int result = deflateInit2(wasm_zlib_buffer.stream,
                            level,                        /* compression level */
                            Z_DEFLATED,                   /* method */
                            PNG_WASM_ZLIB_WINDOW_BITS,    /* window bits */
                            PNG_WASM_ZLIB_MEM_LEVEL,      /* mem level for WASM */
                            strategy);                     /* strategy */

   if (result != Z_OK)
   {
      png_wasm_zlib_cleanup_buffer(png_ptr);
      return result;
   }

   /* Set up stream buffers */
   wasm_zlib_buffer.stream->next_out = wasm_zlib_buffer.output_buffer;
   wasm_zlib_buffer.stream->avail_out = (uInt)wasm_zlib_buffer.output_size;

   PNG_UNUSED(method)
   PNG_UNUSED(window_bits)
   PNG_UNUSED(mem_level)

   return Z_OK;
}

/* PRIORITY 2: WASM-optimized PNG decompression initialization */
PNG_FUNCTION(int, png_wasm_zlib_decompress_init,
            (png_structp png_ptr, int window_bits), PNG_EMPTY)
{
   if (png_ptr == NULL)
      return Z_STREAM_ERROR;

   /* Initialize WASM zlib buffer */
   if (!png_wasm_zlib_init_buffer(png_ptr, Z_DEFAULT_COMPRESSION))
      return Z_MEM_ERROR;

   /* Initialize inflate stream with WASM-optimized parameters */
   int result = inflateInit2(wasm_zlib_buffer.stream, PNG_WASM_ZLIB_WINDOW_BITS);

   if (result != Z_OK)
   {
      png_wasm_zlib_cleanup_buffer(png_ptr);
      return result;
   }

   /* Set up stream buffers */
   wasm_zlib_buffer.stream->next_in = wasm_zlib_buffer.input_buffer;
   wasm_zlib_buffer.stream->avail_in = 0;

   PNG_UNUSED(window_bits)

   return Z_OK;
}

/* PRIORITY 2: WASM-optimized streaming compression */
PNG_FUNCTION(int, png_wasm_zlib_compress_chunk,
            (png_structp png_ptr, png_const_bytep input, png_size_t input_len,
             png_bytep output, png_size_t* output_len, int flush), PNG_EMPTY)
{
   int result = Z_OK;
   png_size_t total_output = 0;

   if (png_ptr == NULL || !wasm_zlib_buffer.initialized ||
       wasm_zlib_buffer.stream == NULL)
      return Z_STREAM_ERROR;

   /* Process input in chunks to manage memory efficiently */
   png_size_t remaining_input = input_len;
   png_const_bytep current_input = input;

   while (remaining_input > 0 || flush == Z_FINISH)
   {
      /* Copy input to buffer if needed */
      if (remaining_input > 0)
      {
         png_size_t chunk_size = (remaining_input > PNG_WASM_ZLIB_CHUNK_SIZE) ?
                                PNG_WASM_ZLIB_CHUNK_SIZE : remaining_input;

         /* Set up input for this chunk */
         wasm_zlib_buffer.stream->next_in = (Bytef*)current_input;
         wasm_zlib_buffer.stream->avail_in = (uInt)chunk_size;

         current_input += chunk_size;
         remaining_input -= chunk_size;
      }

      /* Process compression */
      do
      {
         /* Set up output buffer */
         png_size_t available_output = *output_len - total_output;
         wasm_zlib_buffer.stream->next_out = output + total_output;
         wasm_zlib_buffer.stream->avail_out = (uInt)available_output;

         /* Perform compression */
         int current_flush = (remaining_input == 0) ? flush : Z_NO_FLUSH;
         result = deflate(wasm_zlib_buffer.stream, current_flush);

         if (result != Z_OK && result != Z_STREAM_END)
            break;

         /* Update output counter */
         total_output += available_output - wasm_zlib_buffer.stream->avail_out;

         /* Allow WASM event loop to run for large compressions */
#ifdef __EMSCRIPTEN__
         if (total_output > PNG_WASM_ZLIB_CHUNK_SIZE)
         {
            emscripten_sleep(0);
         }
#endif

      } while (wasm_zlib_buffer.stream->avail_out == 0 && result != Z_STREAM_END);

      if (result != Z_OK && result != Z_STREAM_END)
         break;

      if (flush == Z_FINISH && result == Z_STREAM_END)
         break;
   }

   *output_len = total_output;
   return result;
}

/* PRIORITY 2: WASM-optimized streaming decompression */
PNG_FUNCTION(int, png_wasm_zlib_decompress_chunk,
            (png_structp png_ptr, png_const_bytep input, png_size_t input_len,
             png_bytep output, png_size_t* output_len), PNG_EMPTY)
{
   int result = Z_OK;
   png_size_t total_output = 0;

   if (png_ptr == NULL || !wasm_zlib_buffer.initialized ||
       wasm_zlib_buffer.stream == NULL)
      return Z_STREAM_ERROR;

   /* Set up input */
   wasm_zlib_buffer.stream->next_in = (Bytef*)input;
   wasm_zlib_buffer.stream->avail_in = (uInt)input_len;

   /* Process decompression in chunks */
   while (wasm_zlib_buffer.stream->avail_in > 0 && total_output < *output_len)
   {
      /* Set up output buffer */
      png_size_t available_output = *output_len - total_output;
      png_size_t chunk_output = (available_output > PNG_WASM_ZLIB_CHUNK_SIZE) ?
                               PNG_WASM_ZLIB_CHUNK_SIZE : available_output;

      wasm_zlib_buffer.stream->next_out = output + total_output;
      wasm_zlib_buffer.stream->avail_out = (uInt)chunk_output;

      /* Perform decompression */
      result = inflate(wasm_zlib_buffer.stream, Z_NO_FLUSH);

      if (result != Z_OK && result != Z_STREAM_END)
         break;

      /* Update output counter */
      total_output += chunk_output - wasm_zlib_buffer.stream->avail_out;

      /* Allow WASM event loop to run */
#ifdef __EMSCRIPTEN__
      if (total_output > PNG_WASM_ZLIB_CHUNK_SIZE)
      {
         emscripten_sleep(0);
      }
#endif

      if (result == Z_STREAM_END)
         break;
   }

   *output_len = total_output;
   return result;
}

/* PRIORITY 2: Get zlib compression statistics for performance monitoring */
PNG_FUNCTION(void, png_wasm_zlib_get_stats,
            (png_structp png_ptr, png_wasm_zlib_stats* stats), PNG_EMPTY)
{
   if (stats == NULL)
      return;

   memset(stats, 0, sizeof(png_wasm_zlib_stats));

   if (!wasm_zlib_buffer.initialized || wasm_zlib_buffer.stream == NULL)
      return;

   stats->total_input = wasm_zlib_buffer.stream->total_in;
   stats->total_output = wasm_zlib_buffer.stream->total_out;
   stats->compression_level = wasm_zlib_buffer.compression_level;
   stats->buffer_input_size = wasm_zlib_buffer.input_size;
   stats->buffer_output_size = wasm_zlib_buffer.output_size;

   if (wasm_zlib_buffer.stream->total_in > 0)
   {
      stats->compression_ratio = (float)wasm_zlib_buffer.stream->total_out /
                                (float)wasm_zlib_buffer.stream->total_in;
   }

   PNG_UNUSED(png_ptr)
}

/* PRIORITY 2: Optimize zlib parameters for WASM environment */
PNG_FUNCTION(void, png_wasm_zlib_optimize_params,
            (png_structp png_ptr, png_wasm_zlib_params* params), PNG_EMPTY)
{
   if (params == NULL)
      return;

   /* Set WASM-optimized default parameters */
   params->compression_level = Z_DEFAULT_COMPRESSION;  /* Balance of speed vs compression */
   params->window_bits = PNG_WASM_ZLIB_WINDOW_BITS;    /* Standard window */
   params->mem_level = PNG_WASM_ZLIB_MEM_LEVEL;        /* WASM memory level */
   params->strategy = Z_FILTERED;                      /* Good for PNG data */
   params->buffer_size = PNG_WASM_ZLIB_BUFFER_SIZE;    /* WASM buffer size */
   params->chunk_size = PNG_WASM_ZLIB_CHUNK_SIZE;      /* Processing chunk size */

   /* Adjust parameters based on available memory */
#ifdef __EMSCRIPTEN__
   size_t heap_size = emscripten_get_heap_size();
   size_t used_heap = emscripten_get_heap_usage();

   /* If memory is tight, use lower memory settings */
   if (used_heap > heap_size * 0.7)  /* 70% threshold */
   {
      params->mem_level = 6;  /* Use less memory */
      params->buffer_size = PNG_WASM_ZLIB_BUFFER_SIZE / 2;  /* Smaller buffers */
   }
#endif

   PNG_UNUSED(png_ptr)
}

#endif /* PNG_WASM_SIMD_OPT > 0 || defined(__EMSCRIPTEN__) */
#endif /* PNG_READ_SUPPORTED || PNG_WRITE_SUPPORTED */
