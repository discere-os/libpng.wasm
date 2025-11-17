/* png_wasm_filter_init.c - WASM SIMD filter initialization
 *
 * Copyright (c) 2025 libpng contributors
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * This file provides the PNG_FILTER_OPTIMIZATIONS hook to install
 * WASM SIMD optimized filter functions into libpng.
 */

#ifdef PNG_READ_SUPPORTED

#if PNG_WASM_SIMD_OPT > 0

#define PNG_INTERNAL
#include "../pngpriv.h"

/* Forward declarations from filter_wasm_simd.c */
void png_read_filter_row_up_wasm_simd(png_row_infop row_info,
                                      png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_sub3_wasm_simd(png_row_infop row_info,
                                        png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_sub4_wasm_simd(png_row_infop row_info,
                                        png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_sub6_wasm_simd(png_row_infop row_info,
                                        png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_sub8_wasm_simd(png_row_infop row_info,
                                        png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_avg3_wasm_simd(png_row_infop row_info,
                                        png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_avg4_wasm_simd(png_row_infop row_info,
                                        png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_avg6_wasm_simd(png_row_infop row_info,
                                        png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_avg8_wasm_simd(png_row_infop row_info,
                                        png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_paeth3_wasm_simd(png_row_infop row_info,
                                          png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_paeth4_wasm_simd(png_row_infop row_info,
                                          png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_paeth6_wasm_simd(png_row_infop row_info,
                                          png_bytep row, png_const_bytep prev_row);
void png_read_filter_row_paeth8_wasm_simd(png_row_infop row_info,
                                          png_bytep row, png_const_bytep prev_row);

/* This function is called by png_init_filter_functions in pngrutil.c
 * when PNG_FILTER_OPTIMIZATIONS is defined to png_init_filter_functions_wasm_simd
 */
void
png_init_filter_functions_wasm_simd(png_structp pp, unsigned int bpp)
{
   /* UP filter is always beneficial with SIMD - no dependencies between bytes */
   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_wasm_simd;

   /* SUB, AVG, and Paeth filters - use SIMD based on bytes per pixel */
   switch (bpp)
   {
      case 3:
         pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub3_wasm_simd;
         pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg3_wasm_simd;
         pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth3_wasm_simd;
         break;

      case 4:
         pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub4_wasm_simd;
         pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg4_wasm_simd;
         pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth4_wasm_simd;
         break;

      case 6:
         pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub6_wasm_simd;
         pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg6_wasm_simd;
         pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth6_wasm_simd;
         break;

      case 8:
         pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub8_wasm_simd;
         pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg8_wasm_simd;
         pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth8_wasm_simd;
         break;

      default:
         /* For other bpp values, keep the default scalar implementations */
         break;
   }
}

#endif /* PNG_WASM_SIMD_OPT > 0 */
#endif /* PNG_READ_SUPPORTED */
