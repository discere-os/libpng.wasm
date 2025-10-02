project('libpng.wasm', 'c', version: '1.6.44', meson_version: '>=1.3.0')

simd = get_option('simd')

incs = include_directories('.')

png_sources = files(
  'png.c','pngerror.c','pngget.c','pngmem.c','pngpread.c','pngread.c','pngrio.c','pngrtran.c','pngrutil.c','pngset.c','pngtrans.c','pngwio.c','pngwrite.c','pngwtran.c','pngwutil.c'
)

cargs = ['-DPNG_WASM_SIMD_OPT=0']
# SIMD temporarily disabled - causes filter corruption
# if simd
#   cargs += ['-msimd128']
# endif

# MAIN links to Emscripten zlib
executable('libpng-main',
  files('wasm/libpng_wasm_module.c', 'wasm/wasm_init.c') + png_sources,
  include_directories: incs,
  c_args: cargs,
  link_args: [
    '-sUSE_ZLIB=1','-sWASM=1','-sMODULARIZE=1','-sEXPORT_ES6=1','-sEXPORT_NAME=LibpngModule',
    '-sALLOW_MEMORY_GROWTH=1','-sNO_FILESYSTEM=1',
    '-sEXPORTED_RUNTIME_METHODS=["cwrap","ccall","UTF8ToString"]',
    '-sEXPORTED_FUNCTIONS=["_libpng_wasm_version","_png_wasm_get_version","_png_wasm_init","_png_wasm_get_simd_supported","_png_wasm_get_simd_capabilities","_png_wasm_get_last_error","_png_wasm_decode_buffer","_png_wasm_encode_buffer","_png_wasm_free_buffer","_malloc","_free"]',
    '-O3','-flto'
  ],
  install: true,
  install_dir: get_option('bindir')
)

# SIDE relies on host to provide zlib; allow unresolved symbols
shared_module('libpng-side',
  files('wasm/libpng_wasm_side.c') + png_sources,
  include_directories: incs,
  name_prefix: '',
  name_suffix: 'wasm',
  c_args: cargs,
  link_args: ['-sSIDE_MODULE=2','-fPIC','-O3','-flto','-sERROR_ON_UNDEFINED_SYMBOLS=0','-sEXPORTED_FUNCTIONS=["_libpng_wasm_version"]'],
  install: true,
  install_dir: get_option('libdir')
)

# Static library for linking by dependent libraries
png_lib = static_library('png',
  png_sources,
  include_directories: incs,
  c_args: cargs,
  install: true,
  install_dir: get_option('libdir')
)

# Install headers for dependency chain
install_headers(
  'png.h',
  'pngconf.h',
  'pnglibconf.h',
  subdir: '.'
)

# Generate and install pkg-config file
pkg = import('pkgconfig')
pkg.generate(
  name: 'libpng',
  description: 'PNG library',
  version: meson.project_version(),
  libraries: '-lpng',
  requires: 'zlib',
  subdirs: '.'
)

