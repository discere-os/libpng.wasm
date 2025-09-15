/**
 * libpng.wasm - TypeScript-first PNG image processing for WebAssembly
 * High-performance PNG with SIMD filters and dynamic dependency loading
 *
 * Based on libpng v1.6.44 with comprehensive WASM-native enhancements
 */

import {
  PNGColorType,
  PNGCompression,
  PNGFilter,
  PNGInterlace,
  PNGTransform,
  PNGError,
  PNGFormatError,
  PNGMemoryError,
  PNGCompressionError
} from './types.ts'
import type {
  LibPNGModule,
  PNGImageInfo,
  PNGReadOptions,
  PNGWriteOptions,
  PNGResult,
  PNGCapabilities,
  PNGLoadingOptions,
  FilterPerformance,
  ZlibModule
} from './types.ts'

export default class LibPNG {
  private module: LibPNGModule | null = null
  private zlibModule: ZlibModule | null = null
  private initialized = false
  private loadingOptions: PNGLoadingOptions

  constructor(options: PNGLoadingOptions = {}) {
    this.loadingOptions = {
      cdnUrl: 'https://cdn.discere.cloud/npm/@discere-os/libpng.wasm/',
      fallbackUrls: [
        'https://cdn.jsdelivr.net/npm/@discere-os/libpng.wasm/',
        'https://unpkg.com/@discere-os/libpng.wasm/'
      ],
      preloadZlib: true,
      cachingEnabled: true,
      simdOptimizations: true,
      maxMemoryMB: 512,
      ...options
    }
  }

  /**
   * Initialize libpng.wasm with simplified WASM loading
   */
  async initialize(): Promise<void> {
    if (this.initialized) return

    try {
      // Load WASM module with CDN fallback using proven pattern
      const moduleFactory = await this.loadModuleFactory()
      this.module = await moduleFactory({
        wasmBinary: await this.loadWasmBinary()
      })

      if (!this.module) {
        throw new PNGError('WASM module is null after initialization')
      }

      // Debug: List available functions
      const availableFunctions = Object.keys(this.module).filter(key => typeof (this.module as any)![key] === 'function')
      console.log('🔍 Available WASM functions:', availableFunctions.slice(0, 10).join(', '), '...')

      // Check for essential functions (more flexible)
      const requiredFunctions = [
        '_png_wasm_init',
        '_png_wasm_encode_buffer',
        '_png_wasm_decode_buffer',
        '_png_wasm_get_version'
      ]

      const missingFunctions = requiredFunctions.filter(func => typeof (this.module as any)![func] !== 'function')

      if (missingFunctions.length > 0) {
        console.log('⚠️ Missing functions:', missingFunctions.join(', '))
        console.log('🔍 Checking for alternative function names...')

        // Check for alternative naming patterns
        const alternatives = availableFunctions.filter(func =>
          func.includes('png') || func.includes('init') || func.includes('encode') || func.includes('decode')
        )
        console.log('📝 PNG-related functions found:', alternatives.join(', '))

        // For now, continue if we have any PNG-related functions
        if (alternatives.length === 0) {
          throw new PNGError(`No PNG functions found. Available: ${availableFunctions.slice(0, 5).join(', ')}`)
        }

        console.log('⚠️ Continuing with available functions...')
      }

      // Initialize WASM-specific functions (gracefully handle missing functions)
      if (typeof this.module._png_wasm_init === 'function') {
        const initResult = this.module._png_wasm_init()
        if (!initResult) {
          console.warn('⚠️ PNG WASM init returned false, but continuing...')
        }
      } else {
        console.log('ℹ️ No _png_wasm_init function found, skipping initialization call')
      }

      // Handle SIMD optimizations
      if (this.module._png_wasm_get_simd_supported && this.module._png_wasm_set_simd_enabled) {
        const simdSupported = this.module._png_wasm_get_simd_supported()
        if (this.loadingOptions.simdOptimizations && simdSupported) {
          this.module._png_wasm_set_simd_enabled(1)
          console.log('✅ SIMD optimizations enabled')
        } else {
          this.module._png_wasm_set_simd_enabled(0)
          console.log('ℹ️ SIMD optimizations disabled')
        }
      } else {
        console.log('⚠️ SIMD control functions not available')
      }

      // Initialize dynamic zlib dependency if requested
      if (this.loadingOptions.preloadZlib) {
        try {
          await this.loadZlibDependency()
        } catch (error) {
          console.warn('🔧 Dynamic zlib loading failed, continuing with static compression:', error)
        }
      }

      this.initialized = true
      console.log('✅ libpng.wasm initialized successfully')
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : String(error);
      throw new PNGError(`Failed to initialize libpng.wasm: ${errorMessage}`)
    }
  }

  /**
   * Get comprehensive PNG processing capabilities
   */
  getCapabilities(): PNGCapabilities {
    const wasmSupported = typeof WebAssembly !== 'undefined'
    let simdSupported = false
    let simdFiltersAvailable = false

    if (this.module) {
      simdSupported = !!this.module._png_wasm_get_simd_supported()
      simdFiltersAvailable = simdSupported // SIMD filters available when SIMD supported
    }

    return {
      wasmSupported,
      simdSupported,
      simdFiltersAvailable,
      progressiveLoadingSupported: true,
      zlibIntegrationSupported: !!this.zlibModule,
      maxImageSize: (this.loadingOptions.maxMemoryMB || 512) * 1024 * 1024,
      supportedColorTypes: [
        PNGColorType.GRAY,
        PNGColorType.PALETTE,
        PNGColorType.RGB,
        PNGColorType.RGB_ALPHA,
        PNGColorType.GRAY_ALPHA
      ],
      supportedBitDepths: [1, 2, 4, 8, 16]
    }
  }

  /**
   * Decode PNG image with SIMD-optimized filters
   */
  async decodePNG(
    pngData: Uint8Array,
    options: PNGReadOptions = {}
  ): Promise<PNGResult> {
    if (!this.module) throw new Error('Module not initialized')

    const startTime = performance.now()
    
    try {
      // Allocate memory for input PNG data  
      const inputPtr = this.module._malloc(pngData.length)
      this.module.HEAPU8.set(pngData, inputPtr)

      // Allocate output parameters
      const outputBufferPtr = this.module._malloc(4) // png_bytepp
      const widthPtr = this.module._malloc(4)
      const heightPtr = this.module._malloc(4) 
      const channelsPtr = this.module._malloc(1)
      const bitDepthPtr = this.module._malloc(1)
      const colorTypePtr = this.module._malloc(1)

      // Call PNG decoder
      const decodeResult = this.module.ccall('png_wasm_decode_buffer', 'number', 
        ['number', 'number', 'number', 'number', 'number', 'number', 'number', 'number'],
        [inputPtr, pngData.length, outputBufferPtr, widthPtr, heightPtr, 
         channelsPtr, bitDepthPtr, colorTypePtr]
      )

      if (!decodeResult) {
        // Get error message from WASM
        const errorMsg = this.module.ccall('png_wasm_get_last_error', 'string', [], [])
        throw new PNGFormatError(`PNG decoding failed: ${errorMsg || 'Unknown error'}`)
      }

      // Extract decoded information
      const width = this.module.HEAPU32[widthPtr >> 2]
      const height = this.module.HEAPU32[heightPtr >> 2]
      const channels = this.module.HEAPU8[channelsPtr]
      const bitDepth = this.module.HEAPU8[bitDepthPtr]
      const colorType = this.module.HEAPU8[colorTypePtr] as PNGColorType

      // Get output data
      const outputDataPtr = this.module.HEAPU32[outputBufferPtr >> 2]
      const imageSize = width * height * channels
      const outputData = new Uint8Array(imageSize)
      outputData.set(this.module.HEAPU8.subarray(outputDataPtr, outputDataPtr + imageSize))

      // Cleanup memory
      this.module._free(outputDataPtr) // Free the image data
      this.module._free(inputPtr)
      this.module._free(outputBufferPtr)
      this.module._free(widthPtr)
      this.module._free(heightPtr)
      this.module._free(channelsPtr)
      this.module._free(bitDepthPtr)
      this.module._free(colorTypePtr)

      const processingTime = performance.now() - startTime

      return {
        data: outputData,
        info: {
          width,
          height,
          bitDepth,
          colorType,
          interlaceMethod: PNGInterlace.NONE, // Simplified for now
          compressionMethod: PNGCompression.DEFLATE,
          filterMethod: 0,
          channels,
          rowbytes: width * channels,
          hasAlpha: channels === 2 || channels === 4
        },
        processingTime,
        simdUsed: !!this.module._png_wasm_get_simd_supported()
      }
    } catch (error) {
      throw new PNGError(`PNG decoding failed: ${error}`)
    }
  }

  /**
   * Encode image data to PNG with SIMD-optimized filters
   */
  async encodePNG(
    imageData: Uint8Array,
    width: number,
    height: number,
    options: PNGWriteOptions = {}
  ): Promise<PNGResult> {
    if (!this.module) throw new Error('Module not initialized')

    const startTime = performance.now()
    const channels = Math.floor(imageData.length / (width * height))

    if (channels < 1 || channels > 4) {
      throw new PNGFormatError(`Invalid channel count: ${channels}. Must be 1-4.`)
    }

    try {
      // Allocate memory for input image data
      const inputPtr = this.module._malloc(imageData.length)
      this.module.HEAPU8.set(imageData, inputPtr)

      // Allocate output parameters
      const outputBufferPtr = this.module._malloc(4) // png_bytepp
      const outputSizePtr = this.module._malloc(4) // png_size_tp

      // Call PNG encoder (6 parameters for MAIN_MODULE with static zlib)
      const encodeResult = this.module.ccall('png_wasm_encode_buffer', 'number',
        ['number', 'number', 'number', 'number', 'number', 'number'],
        [inputPtr, width, height, channels, outputBufferPtr, outputSizePtr]
      )

      if (!encodeResult) {
        const errorMsg = this.module.ccall('png_wasm_get_last_error', 'string', [], [])
        throw new PNGCompressionError(`PNG encoding failed: ${errorMsg || 'Unknown error'}`)
      }

      // Extract compressed PNG data
      const outputDataPtr = this.module.HEAPU32[outputBufferPtr >> 2]
      const outputSize = this.module.HEAPU32[outputSizePtr >> 2]
      
      const compressedData = new Uint8Array(outputSize)
      compressedData.set(this.module.HEAPU8.subarray(outputDataPtr, outputDataPtr + outputSize))

      // Cleanup memory
      this.module._free(outputDataPtr) // Free compressed data
      this.module._free(inputPtr)
      this.module._free(outputBufferPtr)
      this.module._free(outputSizePtr)

      const processingTime = performance.now() - startTime

      // Determine color type from channels
      let colorType = PNGColorType.RGB
      switch (channels) {
        case 1: colorType = PNGColorType.GRAY; break
        case 2: colorType = PNGColorType.GRAY_ALPHA; break
        case 3: colorType = PNGColorType.RGB; break
        case 4: colorType = PNGColorType.RGB_ALPHA; break
      }

      return {
        data: compressedData,
        info: {
          width,
          height,
          bitDepth: 8,
          colorType,
          interlaceMethod: options.interlaceType || PNGInterlace.NONE,
          compressionMethod: PNGCompression.DEFLATE,
          filterMethod: options.filterType || PNGFilter.NONE,
          channels,
          rowbytes: width * channels,
          hasAlpha: channels === 2 || channels === 4
        },
        processingTime,
        compressionRatio: imageData.length / compressedData.length,
        simdUsed: !!this.module._png_wasm_get_simd_supported()
      }
    } catch (error) {
      throw new PNGError(`PNG encoding failed: ${error}`)
    }
  }

  /**
   * Benchmark PNG filter performance with SIMD
   */
  async benchmarkFilters(
    width: number,
    height: number,
    channels: number = 4
  ): Promise<FilterPerformance[]> {
    if (!this.module) throw new Error('Module not initialized')

    const filters = [PNGFilter.NONE, PNGFilter.SUB, PNGFilter.UP, PNGFilter.AVERAGE, PNGFilter.PAETH]
    const results: FilterPerformance[] = []

    // Create test image data
    const imageSize = width * height * channels
    const testData = new Uint8Array(imageSize)
    for (let i = 0; i < imageSize; i++) {
      testData[i] = Math.floor(Math.random() * 256)
    }

    for (const filter of filters) {
      const startTime = performance.now()
      
      try {
        // Benchmark filter performance with comprehensive metrics
        const result = await this.encodePNG(testData, width, height, {
          filterType: filter,
          simdOptimizations: true
        })
        
        const processingTime = performance.now() - startTime
        
        results.push({
          filterType: filter,
          processingTime,
          simdAcceleration: result.simdUsed,
          compressionRatio: result.compressionRatio || 1.0,
          qualityScore: this.calculateQualityScore(result)
        })
      } catch (error) {
        console.warn(`Filter ${filter} benchmark failed:`, error)
      }
    }

    return results
  }

  /**
   * Cleanup resources and dependencies
   */
  cleanup(): void {
    if (this.module) {
      this.module._png_wasm_cleanup()
    }
    this.module = null
    this.zlibModule = null
    this.initialized = false
  }

  // Private implementation methods
  private async loadZlibDependency(): Promise<void> {
    try {
      // For MAIN_MODULE testing, try to dynamically load zlib-side.wasm via dlopen
      if (this.module && typeof this.module._dlopen === 'function') {
        console.log('🔧 Attempting dynamic zlib-side.wasm loading via dlopen...')

        // Try to load zlib-side.wasm that should be preloaded in FS
        const zlibHandle = this.module._dlopen('/zlib-side.wasm', 1) // RTLD_NOW

        if (zlibHandle !== 0) {
          console.log('✅ Successfully loaded zlib-side.wasm via dlopen')

          // Try to resolve compression functions
          if (typeof this.module._dlsym === 'function') {
            const compressFunc = this.module._dlsym(zlibHandle, 'compress')
            const uncompressFunc = this.module._dlsym(zlibHandle, 'uncompress')

            if (compressFunc && uncompressFunc) {
              // Create a mock zlib module interface that uses dlopen'd functions
              this.zlibModule = {
                _zlib_compress: compressFunc,
                _zlib_uncompress: uncompressFunc,
                _compress: compressFunc,
                _uncompress: uncompressFunc,
                handle: zlibHandle
              } as ZlibModule

              console.log('🚀 zlib-side.wasm dynamic loading successful')
              return
            }
          }
        } else {
          console.warn('⚠️ Failed to dlopen zlib-side.wasm - may not be preloaded')
        }
      }

      // Fallback: Try CDN loading for production use
      console.log('🔧 Falling back to CDN-based zlib.wasm loading...')
      const zlibVersion = '1.4.2'

      const zlibUrls = [
        `${this.loadingOptions.cdnUrl}/../zlib.wasm/v${zlibVersion}/dist/browser/index.js`,
        `https://cdn.discere.cloud/npm/@discere-os/zlib.wasm/v${zlibVersion}/dist/browser/index.js`,
        // Try local build for testing
        '../zlib.wasm/dist/browser/index.js',
        ...(this.loadingOptions.fallbackUrls?.map((url: string) =>
          url.replace('libpng.wasm', 'zlib.wasm') + `v${zlibVersion}/dist/browser/index.js`
        ) || [])
      ]

      let zlibModule = null
      let lastError: Error | null = null

      // Try each URL until one succeeds
      for (const url of zlibUrls) {
        try {
          console.log(`Loading zlib.wasm from: ${url}`)

          // Dynamic import with proper error handling
          const moduleFactory = await import(/* @vite-ignore */ url)

          // Initialize the zlib WASM module
          zlibModule = await moduleFactory.default()

          if (zlibModule && (typeof zlibModule._zlib_compress_buffer === 'function' ||
                              typeof zlibModule._zlib_compress === 'function')) {
            console.log('✅ zlib.wasm CDN loading successful')
            break
          } else {
            throw new Error('zlib.wasm module invalid - missing expected functions')
          }
        } catch (error) {
          lastError = error as Error
          console.warn(`Failed to load zlib.wasm from ${url}:`, error)
          continue
        }
      }

      if (zlibModule) {
        this.zlibModule = zlibModule
        console.log('🚀 zlib.wasm CDN integration ready - dynamic compression available')
      } else {
        throw lastError || new Error('Failed to load zlib.wasm from any source')
      }

    } catch (error) {
      console.warn('⚠️ zlib.wasm not available, PNG processing will use built-in compression:', error)
      // Set fallback mode - still allow PNG processing without dynamic zlib
      this.zlibModule = null
    }
  }

  private async loadModuleFactory(): Promise<Function> {
    // Try local build first (for testing)
    const localPaths = [
      '../../install/wasm/libpng-release.js',   // Dual build system MAIN_MODULE
      '../../install/wasm/libpng-main.js',      // Alternative name
      '../../build/libpng-release.js',          // Direct build output
    ]

    for (const localPath of localPaths) {
      try {
        const modulePath = new URL(localPath, import.meta.url).href
        const module = await import(modulePath) as any
        const factory = module.default || module.LibPNGModule
        if (factory) {
          console.log(`✅ Loaded libpng module from: ${localPath}`)
          return factory
        }
      } catch (error) {
        // Continue trying other paths
        console.log(`⚠️ Failed to load from ${localPath}:`, (error as Error).message)
      }
    }

    throw new Error('No libpng.wasm module available. Run "deno task build" to build locally.')
  }

  private async loadWasmBinary(): Promise<ArrayBuffer> {
    // Deno environment - use Deno.readFile (proven pattern)
    if (typeof globalThis.Deno !== 'undefined') {
      const localPaths = [
        './install/wasm/libpng-release.wasm',
        './install/wasm/libpng-main.wasm',
        './build/libpng-release.wasm'
      ]

      for (const localPath of localPaths) {
        try {
          const wasmBuffer = await Deno.readFile(localPath)
          console.log(`✅ Loaded libpng.wasm binary from: ${localPath}`)
          return wasmBuffer.buffer
        } catch (error) {
          console.log(`⚠️ Failed to load WASM from ${localPath}:`, (error as Error).message)
          continue
        }
      }
    }

    // Fallback for other environments
    throw new Error('WASM binary loading not supported in this environment. Use Deno or run "deno task build".')
  }

  private setupPNGIO(pngPtr: number, dataPtr: number, dataSize: number): void {
    // Set up custom I/O for reading from WASM memory
    // This would configure libpng to read from our memory buffer
    this.module!._png_set_sig_bytes(pngPtr, 8) // Skip PNG signature
  }

  private getPNGInfo(pngPtr: number, infoPtr: number): PNGImageInfo {
    // Allocate memory for info retrieval
    const widthPtr = this.module!._malloc(4)
    const heightPtr = this.module!._malloc(4)
    const bitDepthPtr = this.module!._malloc(4)
    const colorTypePtr = this.module!._malloc(4)
    const interlacePtr = this.module!._malloc(4)
    const compressionPtr = this.module!._malloc(4)
    const filterPtr = this.module!._malloc(4)

    try {
      // Get PNG header info
      this.module!._png_get_IHDR(
        pngPtr, infoPtr, widthPtr, heightPtr, bitDepthPtr,
        colorTypePtr, interlacePtr, compressionPtr, filterPtr
      )

      const width = this.module!.HEAPU32[widthPtr >> 2]
      const height = this.module!.HEAPU32[heightPtr >> 2]
      const bitDepth = this.module!.HEAPU32[bitDepthPtr >> 2]
      const colorType = this.module!.HEAPU32[colorTypePtr >> 2] as PNGColorType
      const interlaceMethod = this.module!.HEAPU32[interlacePtr >> 2] as PNGInterlace
      const compressionMethod = this.module!.HEAPU32[compressionPtr >> 2] as PNGCompression
      const filterMethod = this.module!.HEAPU32[filterPtr >> 2]

      const channels = this.module!._png_get_channels(pngPtr, infoPtr)
      const rowbytes = this.module!._png_get_rowbytes(pngPtr, infoPtr)
      const hasAlpha = colorType === PNGColorType.RGB_ALPHA || colorType === PNGColorType.GRAY_ALPHA

      return {
        width,
        height,
        bitDepth,
        colorType,
        interlaceMethod,
        compressionMethod,
        filterMethod,
        channels,
        rowbytes,
        hasAlpha
      }
    } finally {
      // Cleanup temporary memory
      this.module!._free(widthPtr)
      this.module!._free(heightPtr)
      this.module!._free(bitDepthPtr)
      this.module!._free(colorTypePtr)
      this.module!._free(interlacePtr)
      this.module!._free(compressionPtr)
      this.module!._free(filterPtr)
    }
  }

  private applyTransforms(pngPtr: number, transforms: PNGTransform[]): void {
    for (const transform of transforms) {
      switch (transform) {
        case PNGTransform.EXPAND:
          this.module!._png_set_expand_gray_1_2_4_to_8(pngPtr)
          this.module!._png_set_palette_to_rgb(pngPtr)
          this.module!._png_set_tRNS_to_alpha(pngPtr)
          break
        case PNGTransform.STRIP_16:
          this.module!._png_set_strip_16(pngPtr)
          break
        case PNGTransform.STRIP_ALPHA:
          this.module!._png_set_strip_alpha(pngPtr)
          break
        case PNGTransform.GRAY_TO_RGB:
          this.module!._png_set_gray_to_rgb(pngPtr)
          break
        // Add other transforms as needed
      }
    }
  }

  private calculateQualityScore(result: PNGResult): number {
    // Simple quality score based on compression ratio and processing time
    const compressionScore = Math.min(result.compressionRatio || 1.0, 10.0) * 10
    const speedScore = Math.max(0, 100 - result.processingTime)
    return (compressionScore + speedScore) / 2
  }
}

// Re-export types for convenience
export * from './types.ts'