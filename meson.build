project('libpng.wasm', 'c', version: '1.6.44', meson_version: '>=1.3.0')

simd = get_option('simd')

incs = include_directories('.')

png_sources = files(
  'png.c','pngerror.c','pngget.c','pngmem.c','pngpread.c','pngread.c','pngrio.c','pngrtran.c','pngrutil.c','pngset.c','pngtrans.c','pngwio.c','pngwrite.c','pngwtran.c','pngwutil.c'
)

wasm_simd_sources = files(
  'wasm/filter_wasm_simd.c',
  'wasm/png_wasm_filter_init.c'
)

cargs = []
if simd
  cargs += ['-DPNG_WASM_SIMD_OPT=1', '-DPNG_FILTER_OPTIMIZATIONS=png_init_filter_functions_wasm_simd', '-msimd128']
else
  cargs += ['-DPNG_WASM_SIMD_OPT=0']
endif

# Determine sources based on SIMD option
main_sources = files('wasm/libpng_wasm_module.c', 'wasm/wasm_init.c') + png_sources
if simd
  main_sources += wasm_simd_sources
endif

# MAIN links to Emscripten zlib
link_args_main = [
  '-sUSE_ZLIB=1','-sWASM=1','-sMODULARIZE=1','-sEXPORT_ES6=1','-sEXPORT_NAME=LibpngModule',
  '-sALLOW_MEMORY_GROWTH=1','-sNO_FILESYSTEM=1',
  '-sEXPORTED_RUNTIME_METHODS=["cwrap","ccall","UTF8ToString"]',
  '-sEXPORTED_FUNCTIONS=["_libpng_wasm_version","_png_wasm_get_version","_png_wasm_init","_png_wasm_get_simd_supported","_png_wasm_get_simd_capabilities","_png_wasm_get_last_error","_png_wasm_decode_buffer","_png_wasm_encode_buffer","_png_wasm_free_buffer","_malloc","_free"]',
  '-O3','-flto'
]
if simd
  link_args_main += ['-msimd128']
endif

executable('libpng-main',
  main_sources,
  include_directories: incs,
  c_args: cargs,
  link_args: link_args_main,
  install: true,
  install_dir: get_option('bindir')
)

# SIDE relies on host to provide zlib; allow unresolved symbols
side_sources = files('wasm/libpng_wasm_side.c') + png_sources
if simd
  side_sources += wasm_simd_sources
endif

link_args_side = ['-sSIDE_MODULE=2','-fPIC','-O3','-flto','-sERROR_ON_UNDEFINED_SYMBOLS=0','-sEXPORTED_FUNCTIONS=["_libpng_wasm_version"]']
if simd
  link_args_side += ['-msimd128']
endif

shared_module('libpng-side',
  side_sources,
  include_directories: incs,
  name_prefix: '',
  name_suffix: 'wasm',
  c_args: cargs,
  link_args: link_args_side,
  install: true,
  install_dir: get_option('libdir')
)

# Static library for linking by dependent libraries
lib_sources = png_sources
if simd
  lib_sources += wasm_simd_sources
endif

png_lib = static_library('png',
  lib_sources,
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

# Generate WASM manifest after build
manifest_script = find_program('../../../../scripts/generate-manifest-meson.sh', required: false)
if manifest_script.found()
  run_target('manifest',
    command: [manifest_script, meson.project_source_root(), meson.project_build_root()]
  )
endif

