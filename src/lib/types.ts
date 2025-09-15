/**
 * TypeScript definitions for libpng.wasm v1.6.44
 * Professional WASM-native interface for PNG image processing
 *
 * Based on libpng v1.6.44 with SIMD filter optimizations
 */

// PNG Color Types
export enum PNGColorType {
  GRAY = 0,
  PALETTE = 1,
  RGB = 2,
  RGB_ALPHA = 4,
  GRAY_ALPHA = 6
}

// PNG Compression Methods
export enum PNGCompression {
  DEFLATE = 0
}

// PNG Filter Types
export enum PNGFilter {
  NONE = 0,
  SUB = 1,
  UP = 2,
  AVERAGE = 3,
  PAETH = 4
}

// PNG Interlace Types
export enum PNGInterlace {
  NONE = 0,
  ADAM7 = 1
}

// PNG Transform Flags
export enum PNGTransform {
  IDENTITY = 0x0000,
  STRIP_16 = 0x0001,
  STRIP_ALPHA = 0x0002,
  PACKING = 0x0004,
  PACKSWAP = 0x0008,
  EXPAND = 0x0010,
  INVERT_MONO = 0x0020,
  SHIFT = 0x0040,
  BGR = 0x0080,
  SWAP_ALPHA = 0x0100,
  SWAP_ENDIAN = 0x0200,
  INVERT_ALPHA = 0x0400,
  STRIP_FILLER = 0x0800,
  STRIP_FILLER_BEFORE = 0x1000,
  GRAY_TO_RGB = 0x2000,
  EXPAND_16 = 0x4000,
  SCALE_16 = 0x8000
}

// Core libpng WASM module interface
export interface LibPNGModule {
  // Memory management
  _malloc(size: number): number
  _free(ptr: number): void

  // Core PNG functions (from cwrap bindings)
  _png_create_read_struct(version: string, errorPtr: number, warnPtr: number, errorFn: number): number
  _png_create_write_struct(version: string, errorPtr: number, warnPtr: number, errorFn: number): number
  _png_destroy_read_struct(pngPtr: number, infoPtr: number): void
  _png_destroy_write_struct(pngPtr: number, infoPtr: number): void
  _png_create_info_struct(pngPtr: number): number
  _png_destroy_info_struct(pngPtr: number, infoPtr: number): void

  // PNG I/O functions
  _png_set_sig_bytes(pngPtr: number, numBytes: number): void
  _png_read_info(pngPtr: number, infoPtr: number): void
  _png_read_image(pngPtr: number, rowPointers: number): void
  _png_read_end(pngPtr: number, infoPtr: number): void
  _png_write_info(pngPtr: number, infoPtr: number): void
  _png_write_image(pngPtr: number, rowPointers: number): void
  _png_write_end(pngPtr: number, infoPtr: number): void

  // PNG info functions
  _png_get_IHDR(pngPtr: number, infoPtr: number, width: number, height: number, 
                bitDepth: number, colorType: number, interlaceMethod: number, 
                compressionMethod: number, filterMethod: number): number
  _png_set_IHDR(pngPtr: number, infoPtr: number, width: number, height: number,
                bitDepth: number, colorType: number, interlaceMethod: number,
                compressionMethod: number, filterMethod: number): void
  _png_get_rowbytes(pngPtr: number, infoPtr: number): number
  _png_get_channels(pngPtr: number, infoPtr: number): number
  _png_get_bit_depth(pngPtr: number, infoPtr: number): number
  _png_get_color_type(pngPtr: number, infoPtr: number): number

  // PNG transformation functions
  _png_set_expand_gray_1_2_4_to_8(pngPtr: number): void
  _png_set_palette_to_rgb(pngPtr: number): void
  _png_set_tRNS_to_alpha(pngPtr: number): void
  _png_set_strip_16(pngPtr: number): void
  _png_set_strip_alpha(pngPtr: number): void
  _png_set_gray_to_rgb(pngPtr: number): void
  _png_set_rgb_to_gray(pngPtr: number, errorAction: number, red: number, green: number): number

  // Dynamic loading functions (MAIN_MODULE only)
  _dlopen?(filename: string, flags: number): number
  _dlsym?(handle: number, symbol: string): number
  _dlclose?(handle: number): number

  // WASM-specific optimized functions
  _png_wasm_init(): number
  _png_wasm_cleanup(): void
  _png_wasm_set_simd_enabled(enabled: number): void
  _png_wasm_get_simd_supported(): number
  _png_wasm_compress_optimized(input: number, inputSize: number, output: number, outputSize: number): number
  _png_wasm_decompress_optimized(input: number, inputSize: number, output: number, outputSize: number): number

  // SIMD filter functions (enhanced performance)
  _png_read_filter_row_up_wasm_simd(rowInfo: number, row: number, prevRow: number): void
  _png_read_filter_row_sub_wasm_simd(rowInfo: number, row: number, prevRow: number): void
  _png_read_filter_row_avg_wasm_simd(rowInfo: number, row: number, prevRow: number): void
  _png_read_filter_row_paeth_wasm_simd(rowInfo: number, row: number, prevRow: number): void

  // Error handling (WASM-native)
  _png_wasm_set_error_handler(pngPtr: number, errorCallback: number): void
  _png_wasm_set_warning_handler(pngPtr: number, warnCallback: number): void
  _png_wasm_get_last_error(): string

  // Memory operations (WASM optimized)
  _png_wasm_alloc_rows(height: number, rowbytes: number): number
  _png_wasm_free_rows(rowPointers: number, height: number): void

  // Progressive loading (WASM streaming)
  _png_wasm_progressive_create(): number
  _png_wasm_progressive_destroy(context: number): void
  _png_wasm_progressive_update(context: number, data: number, dataSize: number): number

  // Emscripten runtime methods
  ccall: (ident: string, returnType: string, argTypes: string[], args: unknown[]) => unknown
  cwrap: (ident: string, returnType: string, argTypes: string[]) => Function
  addFunction: (func: Function, signature: string) => number
  removeFunction: (funcPtr: number) => void
  wasmTable: WebAssembly.Table
  UTF8ToString: (ptr: number, maxLength?: number) => string
  stringToUTF8: (str: string, outPtr: number, maxBytesToWrite?: number) => void

  // Memory access
  HEAPU8: Uint8Array
  HEAP8: Int8Array
  HEAPU16: Uint16Array
  HEAP16: Int16Array
  HEAPU32: Uint32Array
  HEAP32: Int32Array
  HEAPF32: Float32Array
  HEAPF64: Float64Array
}

// PNG Image Information
export interface PNGImageInfo {
  width: number
  height: number
  bitDepth: number
  colorType: PNGColorType
  interlaceMethod: PNGInterlace
  compressionMethod: PNGCompression
  filterMethod: number
  channels: number
  rowbytes: number
  hasAlpha: boolean
}

// PNG Read Options
export interface PNGReadOptions {
  transforms?: PNGTransform[]
  gamma?: number
  backgroundColor?: { r: number; g: number; b: number }
  stripTo8Bit?: boolean
  expandPalette?: boolean
  expandGray?: boolean
  addAlpha?: boolean
  simdOptimizations?: boolean
  progressiveCallback?: (bytesLoaded: number, totalBytes: number) => void
}

// PNG Write Options
export interface PNGWriteOptions {
  compressionLevel?: number // 0-9
  filterType?: PNGFilter
  interlaceType?: PNGInterlace
  gamma?: number
  simdOptimizations?: boolean
  progressiveCallback?: (bytesWritten: number, totalBytes: number) => void
}

// PNG Processing Result
export interface PNGResult {
  data: Uint8Array
  info: PNGImageInfo
  processingTime: number
  compressionRatio?: number
  simdUsed: boolean
}

// WASM-native capabilities
export interface PNGCapabilities {
  wasmSupported: boolean
  simdSupported: boolean
  simdFiltersAvailable: boolean
  progressiveLoadingSupported: boolean
  zlibIntegrationSupported: boolean
  maxImageSize: number
  supportedColorTypes: PNGColorType[]
  supportedBitDepths: number[]
}

// Dynamic loading configuration
export interface PNGLoadingOptions {
  cdnUrl?: string
  fallbackUrls?: string[]
  preloadZlib?: boolean
  cachingEnabled?: boolean
  simdOptimizations?: boolean
  maxMemoryMB?: number
}

// Error classes
export class PNGError extends Error {
  constructor(message: string, public readonly code?: number) {
    super(message)
    this.name = 'PNGError'
  }
}

export class PNGFormatError extends PNGError {
  constructor(message: string) {
    super(message)
    this.name = 'PNGFormatError'
  }
}

export class PNGMemoryError extends PNGError {
  constructor(message: string) {
    super(message)
    this.name = 'PNGMemoryError'
  }
}

export class PNGCompressionError extends PNGError {
  constructor(message: string) {
    super(message)
    this.name = 'PNGCompressionError'
  }
}

// Filter performance metrics
export interface FilterPerformance {
  filterType: PNGFilter
  processingTime: number
  simdAcceleration: boolean
  compressionRatio: number
  qualityScore: number
}

// Dynamic zlib.wasm dependency interface
export interface ZlibModule {
  // Standard zlib functions used by libpng
  _zlib_compress_buffer(
    input: number,
    inputLen: number,
    output: number,
    outputLen: number,
    level: number
  ): number
  
  _zlib_decompress_buffer(
    input: number,
    inputLen: number,
    output: number,
    outputLen: number
  ): number

  _zlib_compress_bound(inputLen: number): number
  _zlib_get_version(): number
  _zlib_crc32(crc: number, data: number, len: number): number

  // Memory management
  _malloc(size: number): number
  _free(ptr: number): void

  // Additional dlopen-based interface (optional)
  _zlib_compress?: any
  _zlib_uncompress?: any
  _compress?: any
  _uncompress?: any
  handle?: any

  // Optional optimized functions
  _zlib_compress_optimized?(
    input: number,
    inputLen: number,
    output: number,
    outputLen: number,
    level: number
  ): number
  
  _zlib_init_optimized_memory?(): void
  _zlib_cleanup_optimized_memory?(): void

  // WASM memory view
  HEAPU8: Uint8Array
}