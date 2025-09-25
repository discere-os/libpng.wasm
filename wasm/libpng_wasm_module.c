#include <emscripten.h>
#include "png.h"

EMSCRIPTEN_KEEPALIVE
const char* libpng_wasm_version(void) {
  return png_get_libpng_ver(NULL);
}

