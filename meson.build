project('libpng.wasm', 'c', version: '1.6.44', meson_version: '>=1.3.0')

simd = get_option('simd')

incs = include_directories('.')

png_sources = files(
  'png.c','pngerror.c','pngget.c','pngmem.c','pngpread.c','pngread.c','pngrio.c','pngrtran.c','pngrutil.c','pngset.c','pngtrans.c','pngwio.c','pngwrite.c','pngwtran.c','pngwutil.c'
)

cargs = []
if simd
  cargs += ['-msimd128']
endif

# MAIN links to Emscripten zlib
executable('libpng-main',
  files('wasm/libpng_wasm_module.c') + png_sources,
  include_directories: incs,
  c_args: cargs,
  link_args: [
    '-sUSE_ZLIB=1','-sWASM=1','-sMODULARIZE=1','-sEXPORT_ES6=1','-sEXPORT_NAME=LibpngModule',
    '-sALLOW_MEMORY_GROWTH=1','-sNO_FILESYSTEM=1','-sEXPORTED_RUNTIME_METHODS=["cwrap","ccall","UTF8ToString"]','-O3','-flto'
  ],
  install: true,
  install_dir: get_option('bindir')
)

# SIDE relies on host to provide zlib; allow unresolved symbols
shared_module('libpng-side',
  files('wasm/libpng_wasm_side.c') + png_sources,
  include_directories: incs,
  name_prefix: '',
  c_args: cargs,
  link_args: ['-sSIDE_MODULE=2','-fPIC','-O3','-flto','-sERROR_ON_UNDEFINED_SYMBOLS=0','-sEXPORTED_FUNCTIONS=["_libpng_wasm_version"]'],
  install: true,
  install_dir: get_option('libdir')
)

