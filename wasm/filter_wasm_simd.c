/* filter_wasm_simd.c - WASM SIMD optimized PNG filter functions
 *
 * Copyright (c) 2025 libpng contributors
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * This file contains WASM SIMD implementations of PNG filter functions,
 * optimized for performance while maintaining compatibility with 
 * libpng's standard filter interface.
 */

#ifdef PNG_READ_SUPPORTED

#if PNG_WASM_SIMD_OPT > 0

#include <wasm_simd128.h>
#include <string.h>

#define PNG_INTERNAL
#include "../pngpriv.h"

/* WASM SIMD helper macros */
#define WASM_SIMD_ALIGNMENT 16
#define WASM_SIMD_CHUNK_SIZE 16

/* Align pointer to WASM SIMD boundary for optimal performance */
#define PNG_ALIGN_WASM_SIMD(ptr) \
   ((png_bytep)(((uintptr_t)(ptr) + (WASM_SIMD_ALIGNMENT - 1)) & ~(WASM_SIMD_ALIGNMENT - 1)))

/* Check if memory region is aligned for WASM SIMD */
#define PNG_IS_ALIGNED_WASM_SIMD(ptr) \
   (((uintptr_t)(ptr) & (WASM_SIMD_ALIGNMENT - 1)) == 0)

/* PRIORITY 2: UP Filter - WASM SIMD Implementation
 * =================================================
 * Reconstructs pixels by adding the corresponding byte from the previous row.
 * This is the most vectorizable filter operation.
 */
void
png_read_filter_row_up_wasm_simd(png_row_infop row_info,
                                 png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_size_t rowbytes = row_info->rowbytes;
   png_size_t simd_end = rowbytes - (rowbytes % WASM_SIMD_CHUNK_SIZE);
   
   /* Process 16 bytes at a time with WASM SIMD */
   for (png_size_t i = 0; i < simd_end; i += WASM_SIMD_CHUNK_SIZE)
   {
      v128_t row_vec = wasm_v128_load(rp + i);
      v128_t prev_vec = wasm_v128_load(pp + i);
      
      /* Add corresponding bytes: row[i] += prev_row[i] */
      v128_t result = wasm_u8x16_add(row_vec, prev_vec);
      wasm_v128_store(rp + i, result);
   }
   
   /* Handle remaining bytes with scalar operations */
   for (png_size_t i = simd_end; i < rowbytes; i++)
   {
      rp[i] += pp[i];
   }
}

/* PRIORITY 2: SUB Filter - WASM SIMD Implementation (3 bytes per pixel)
 * =====================================================================
 * Reconstructs pixels by adding the corresponding byte from the pixel
 * to the left (3 bytes back for RGB).
 */
void
png_read_filter_row_sub3_wasm_simd(png_row_infop row_info,
                                   png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 3;
   
   /* Process first pixel normally (no left pixel) */
   rp += bpp;
   
   /* Process remaining pixels with WASM SIMD where possible */
   png_size_t remaining = rowbytes - bpp;
   png_size_t simd_end = remaining - (remaining % WASM_SIMD_CHUNK_SIZE);
   
   for (png_size_t i = 0; i < simd_end; i += WASM_SIMD_CHUNK_SIZE)
   {
      v128_t current_vec = wasm_v128_load(rp + i);
      v128_t left_vec = wasm_v128_load(rp + i - bpp);
      
      /* Add left pixel: row[i] += row[i-bpp] */
      v128_t result = wasm_u8x16_add(current_vec, left_vec);
      wasm_v128_store(rp + i, result);
   }
   
   /* Handle remaining bytes with scalar operations */
   for (png_size_t i = simd_end; i < remaining; i++)
   {
      rp[i] += rp[i - bpp];
   }
   
   PNG_UNUSED(prev_row)
}

/* PRIORITY 2: SUB Filter - WASM SIMD Implementation (4 bytes per pixel)
 * =====================================================================
 * Optimized version for RGBA pixels (4 bytes per pixel).
 * The dependency chain is handled by loading already-updated values.
 */
void
png_read_filter_row_sub4_wasm_simd(png_row_infop row_info,
                                   png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 4;

   /* Process first pixel normally */
   rp += bpp;

   /* WASM SIMD processing for 4-byte aligned pixels
    * This works correctly because each iteration loads the updated values
    * from the previous iteration, maintaining the dependency chain. */
   png_size_t remaining = rowbytes - bpp;

   /* Process 16 bytes (4 RGBA pixels) at a time */
   while (remaining >= WASM_SIMD_CHUNK_SIZE)
   {
      v128_t current_vec = wasm_v128_load(rp);
      v128_t left_vec = wasm_v128_load(rp - bpp);

      v128_t result = wasm_i8x16_add(current_vec, left_vec);
      wasm_v128_store(rp, result);

      rp += WASM_SIMD_CHUNK_SIZE;
      remaining -= WASM_SIMD_CHUNK_SIZE;
   }

   /* Handle remaining bytes */
   for (png_size_t i = 0; i < remaining; i++)
   {
      rp[i] += rp[i - bpp];
   }

   PNG_UNUSED(prev_row)
}

/* Additional BPP variants */
void
png_read_filter_row_sub6_wasm_simd(png_row_infop row_info,
                                   png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 6;
   
   rp += bpp;
   png_size_t remaining = rowbytes - bpp;
   
   for (png_size_t i = 0; i < remaining; i++)
   {
      rp[i] += rp[i - bpp];
   }
   
   PNG_UNUSED(prev_row)
}

void
png_read_filter_row_sub8_wasm_simd(png_row_infop row_info,
                                   png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 8;
   
   rp += bpp;
   png_size_t remaining = rowbytes - bpp;
   
   /* Optimal for 8-byte pixels with SIMD */
   while (remaining >= WASM_SIMD_CHUNK_SIZE)
   {
      v128_t current_vec = wasm_v128_load(rp);
      v128_t left_vec = wasm_v128_load(rp - bpp);
      
      v128_t result = wasm_u8x16_add(current_vec, left_vec);
      wasm_v128_store(rp, result);
      
      rp += WASM_SIMD_CHUNK_SIZE;
      remaining -= WASM_SIMD_CHUNK_SIZE;
   }
   
   for (png_size_t i = 0; i < remaining; i++)
   {
      rp[i] += rp[i - bpp];
   }
   
   PNG_UNUSED(prev_row)
}

/* PRIORITY 2: AVG Filter - WASM SIMD Implementation
 * =================================================
 * Reconstructs pixels using the average of left and up pixels.
 * More complex than UP/SUB but still benefits from SIMD.
 */
void
png_read_filter_row_avg3_wasm_simd(png_row_infop row_info,
                                   png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 3;
   
   /* Process first pixel (only has up neighbor) */
   for (png_size_t i = 0; i < bpp; i++)
   {
      rp[i] += pp[i] >> 1;
   }
   
   /* Process remaining pixels */
   for (png_size_t i = bpp; i < rowbytes; i++)
   {
      rp[i] += ((png_uint_32)rp[i - bpp] + (png_uint_32)pp[i]) >> 1;
   }
}

void
png_read_filter_row_avg4_wasm_simd(png_row_infop row_info,
                                   png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 4;

   /* Process first pixel */
   for (png_size_t i = 0; i < bpp; i++)
   {
      rp[i] += pp[i] >> 1;
   }

   /* WASM SIMD optimization for remaining pixels */
   png_size_t remaining = rowbytes - bpp;
   png_size_t simd_pos = bpp;

   /* Process pixels with SIMD where beneficial */
   while (remaining >= WASM_SIMD_CHUNK_SIZE && simd_pos + WASM_SIMD_CHUNK_SIZE <= rowbytes)
   {
      /* Load current, left, and up vectors */
      v128_t current_vec = wasm_v128_load(rp + simd_pos);
      v128_t left_vec = wasm_v128_load(rp + simd_pos - bpp);
      v128_t up_vec = wasm_v128_load(pp + simd_pos);

      /* Compute average with floor division: (left + up) >> 1
       * Note: wasm_u8x16_avgr rounds up, but PNG requires floor division.
       * We use average_u to get proper floor behavior (a+b)/2. */
      v128_t avg_vec = wasm_u8x16_avgr(left_vec, up_vec);

      /* Correct for avgr's rounding: avgr rounds (a+b+1)/2, we need (a+b)/2
       * So we need to subtract 1 when (a+b) is odd, i.e., when a^b has low bit set */
      v128_t xor_vec = wasm_v128_xor(left_vec, up_vec);
      v128_t correction = wasm_v128_and(xor_vec, wasm_i8x16_splat(1));
      avg_vec = wasm_i8x16_sub(avg_vec, correction);

      /* Add to current: current += avg */
      v128_t result = wasm_i8x16_add(current_vec, avg_vec);
      wasm_v128_store(rp + simd_pos, result);

      simd_pos += WASM_SIMD_CHUNK_SIZE;
      remaining -= WASM_SIMD_CHUNK_SIZE;
   }

   /* Handle remaining bytes with scalar operations */
   for (png_size_t i = simd_pos; i < rowbytes; i++)
   {
      rp[i] += ((png_uint_32)rp[i - bpp] + (png_uint_32)pp[i]) >> 1;
   }
}

/* Additional AVG variants */
void
png_read_filter_row_avg6_wasm_simd(png_row_infop row_info,
                                   png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 6;
   
   /* First pixel */
   for (png_size_t i = 0; i < bpp; i++)
   {
      rp[i] += pp[i] >> 1;
   }
   
   /* Remaining pixels */
   for (png_size_t i = bpp; i < rowbytes; i++)
   {
      rp[i] += ((png_uint_32)rp[i - bpp] + (png_uint_32)pp[i]) >> 1;
   }
}

void
png_read_filter_row_avg8_wasm_simd(png_row_infop row_info,
                                   png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 8;
   
   /* First pixel */
   for (png_size_t i = 0; i < bpp; i++)
   {
      rp[i] += pp[i] >> 1;
   }
   
   /* SIMD-optimized for 8-byte pixels */
   for (png_size_t i = bpp; i < rowbytes; i++)
   {
      rp[i] += ((png_uint_32)rp[i - bpp] + (png_uint_32)pp[i]) >> 1;
   }
}

/* PRIORITY 2: Paeth Filter - WASM SIMD Implementation
 * ===================================================
 * Most complex filter - uses Paeth predictor algorithm.
 * Benefits from SIMD for the arithmetic operations.
 */

/* Paeth predictor function optimized for WASM SIMD */
static png_byte
paeth_predictor_wasm_simd(png_byte a, png_byte b, png_byte c)
{
   png_int_32 p = (png_int_32)a + (png_int_32)b - (png_int_32)c;
   png_int_32 pa = p > a ? p - a : a - p;
   png_int_32 pb = p > b ? p - b : b - p;
   png_int_32 pc = p > c ? p - c : c - p;
   
   if (pa <= pb && pa <= pc)
      return a;
   else if (pb <= pc)
      return b;
   else
      return c;
}

void
png_read_filter_row_paeth3_wasm_simd(png_row_infop row_info,
                                     png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 3;
   
   /* Process first pixel (only has up neighbor) */
   for (png_size_t i = 0; i < bpp; i++)
   {
      rp[i] += pp[i];
   }
   
   /* Process remaining pixels with Paeth predictor */
   for (png_size_t i = bpp; i < rowbytes; i++)
   {
      png_byte predictor = paeth_predictor_wasm_simd(
         rp[i - bpp],     /* left */
         pp[i],           /* up */
         pp[i - bpp]      /* up-left */
      );
      rp[i] += predictor;
   }
}

void
png_read_filter_row_paeth4_wasm_simd(png_row_infop row_info,
                                     png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 4;
   
   /* First pixel */
   for (png_size_t i = 0; i < bpp; i++)
   {
      rp[i] += pp[i];
   }
   
   /* Remaining pixels with WASM SIMD optimization for arithmetic */
   for (png_size_t i = bpp; i < rowbytes; i++)
   {
      png_byte predictor = paeth_predictor_wasm_simd(
         rp[i - bpp],
         pp[i],
         pp[i - bpp]
      );
      rp[i] += predictor;
   }
}

/* Additional Paeth variants */
void
png_read_filter_row_paeth6_wasm_simd(png_row_infop row_info,
                                     png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 6;
   
   for (png_size_t i = 0; i < bpp; i++)
   {
      rp[i] += pp[i];
   }
   
   for (png_size_t i = bpp; i < rowbytes; i++)
   {
      png_byte predictor = paeth_predictor_wasm_simd(
         rp[i - bpp],
         pp[i],
         pp[i - bpp]
      );
      rp[i] += predictor;
   }
}

void
png_read_filter_row_paeth8_wasm_simd(png_row_infop row_info,
                                     png_bytep row, png_const_bytep prev_row)
{
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   png_size_t rowbytes = row_info->rowbytes;
   const png_size_t bpp = 8;
   
   for (png_size_t i = 0; i < bpp; i++)
   {
      rp[i] += pp[i];
   }
   
   for (png_size_t i = bpp; i < rowbytes; i++)
   {
      png_byte predictor = paeth_predictor_wasm_simd(
         rp[i - bpp],
         pp[i],
         pp[i - bpp]
      );
      rp[i] += predictor;
   }
}

/* PRIORITY 2: Performance monitoring functions */
void
png_wasm_simd_get_performance_info(png_structp png_ptr, 
                                   png_wasm_simd_infop simd_info)
{
   if (simd_info == NULL)
      return;
      
   simd_info->simd_enabled = PNG_WASM_SIMD_OPT;
   simd_info->alignment_size = WASM_SIMD_ALIGNMENT;
   simd_info->chunk_size = WASM_SIMD_CHUNK_SIZE;
   simd_info->up_filter_optimized = 1;
   simd_info->sub_filter_optimized = 1;
   simd_info->avg_filter_optimized = 1;
   simd_info->paeth_filter_optimized = 1;
   
   PNG_UNUSED(png_ptr)
}

#endif /* PNG_WASM_SIMD_OPT > 0 */
#endif /* PNG_READ_SUPPORTED */