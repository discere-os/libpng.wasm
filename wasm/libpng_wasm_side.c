#include "png.h"

const char* libpng_wasm_version(void) {
  return png_get_libpng_ver(NULL);
}

