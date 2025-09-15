/* wasm_init.c - WASM SIMD initialization for libpng
 *
 * Copyright (c) 2025 libpng contributors
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

#ifdef PNG_READ_SUPPORTED

#if PNG_WASM_SIMD_OPT > 0

#include <wasm_simd128.h>

#define PNG_INTERNAL
#include "../pngpriv.h"

/* WASM SIMD detection and initialization */

/* Check if WASM SIMD is available at runtime - DISABLED to prevent filter corruption */
int png_have_wasm_simd(void)
{
   /* DISABLED: Return 0 to force libpng to use standard filters
    * WASM SIMD filters cause "IDAT: invalid stored block lengths" errors */
   return 0;
}

/* Initialize WASM SIMD optimizations - DISABLED for MAIN_MODULE to avoid corruption */
void
png_init_filter_functions_wasm_simd(png_structp pp, unsigned int bpp)
{
   /* DISABLED: WASM SIMD filters cause data corruption in MAIN_MODULE build
    * Let libpng use standard filter implementations instead */
   (void)pp;    /* Suppress unused parameter warning */
   (void)bpp;   /* Suppress unused parameter warning */
   return;
}

/* Check if the current build supports WASM SIMD */
int png_wasm_simd_support(void)
{
#if PNG_WASM_SIMD_OPT > 0
   return png_have_wasm_simd();
#else
   return 0;
#endif
}

/* Get WASM SIMD capability information */
void png_get_wasm_simd_flags(png_structp png_ptr, png_uint_32p flags)
{
   if (flags == NULL)
      return;

   *flags = 0;

#if PNG_WASM_SIMD_OPT > 0
   if (png_have_wasm_simd())
   {
      *flags |= PNG_FILTER_OPTIMIZATIONS;
      *flags |= PNG_WASM_SIMD_OPT;
   }
#endif

   PNG_UNUSED(png_ptr)
}

#endif /* PNG_WASM_SIMD_OPT > 0 */

/* Get libpng version string for WASM module - always available */
const char* png_wasm_get_version(void)
{
   return PNG_LIBPNG_VER_STRING;
}

#endif /* PNG_READ_SUPPORTED */