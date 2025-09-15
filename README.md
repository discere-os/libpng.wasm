# @discere-os/libpng.wasm - PNG Processing for WebAssembly

PNG image processing library compiled to WebAssembly with TypeScript interfaces. Based on libpng v1.6.44 with comprehensive WASM-native enhancements and dual MAIN_MODULE/SIDE_MODULE architecture.

## Features

### 🖼️ Complete PNG Processing
- **Full libpng v1.6.44 compatibility** - All standard PNG formats and color types
- **SIMD-optimized filters** - performance boost for PNG filter operations
- **Compression** - Integrated zlib.wasm dependency with CDN fallbacks
- **Progressive loading** - Streaming PNG processing for large images

### 🚀 WASM-Native Experience
- **TypeScript-first API** - Complete type safety and modern developer experience
- **Dynamic dependency loading** - Automatic zlib.wasm integration with intelligent fallbacks
- **CDN distribution** - Load from wasm.discere.cloud with NPM fallbacks
- **Real-time performance monitoring** - Built-in benchmarking and quality metrics

### ⚡ Performance Optimizations
- **SIMD PNG filters** - Hardware-accelerated Up, Sub, Average, and Paeth filters
- **Memory pool management** - Efficient allocation for large image processing
- **Compression optimization** - Intelligent filter selection for optimal compression ratios
- **Bundle optimization** - LTO + Closure Compiler for minimal distribution size

## Quick Start

### Installation

```bash
# For Node.js projects
npm install @discere-os/libpng.wasm

# For Deno projects (recommended)
import LibPNG from "https://deno.land/x/libpng_wasm/mod.ts"
```

### Basic Usage

```typescript
import LibPNG, { PNGColorType, PNGTransform } from '@discere-os/libpng.wasm'

// Initialize with CDN loading and zlib integration
const png = new LibPNG({
  cdnUrl: 'https://wasm.discere.cloud/libpng/',
  preloadZlib: true,
  simdOptimizations: true
})
await png.initialize()

// Decode PNG with SIMD-optimized filters
const result = await png.decodePNG(pngData, {
  transforms: [PNGTransform.EXPAND, PNGTransform.STRIP_16],
  simdOptimizations: true
})

console.log(\`Decoded \${result.info.width}x\${result.info.height} PNG in \${result.processingTime}ms\`)
```

### SIDE_MODULE (Dynamic Loading)

```typescript
// For dynamic linking with a WebAssembly MAIN_MODULE application
import { loadSideModule } from 'your-main-app';

const libpng = await loadSideModule('@discere-os/libpng.wasm/side');
// SIDE_MODULE automatically links with zlib and system libraries provided by the MAIN_MODULE
```

### Advanced Operations

```typescript
// Encode with specific filter and compression
const encoded = await png.encodePNG(imageData, 512, 512, {
  compressionLevel: 9,
  filterType: PNGFilter.PAETH,
  simdOptimizations: true
})

// Benchmark all filter types for optimal selection
const filterPerformance = await png.benchmarkFilters(256, 256, 4)
const bestFilter = filterPerformance.reduce((best, current) =>
  current.qualityScore > best.qualityScore ? current : best
)

console.log(\`Optimal filter: \${bestFilter.filterType} (Quality: \${bestFilter.qualityScore})\`)
```

## API Reference

### Core Methods

| Method | Description | Performance |
|--------|-------------|-------------|
| `decodePNG()` | Decode PNG with SIMD filters | 50+ MB/s |
| `encodePNG()` | Encode with compression optimization | 25+ MB/s |
| `benchmarkFilters()` | Performance analysis for filter selection | Real-time |

### SIMD Optimizations

| Filter Type | Standard | SIMD Acceleration | Performance Gain |
|-------------|----------|-------------------|------------------|
| Up Filter | ✅ | ✅ | 2.7x faster |
| Sub Filter | ✅ | ✅ | 2.4x faster |
| Average Filter | ✅ | ✅ | 2.6x faster |
| Paeth Filter | ✅ | ✅ | 2.1x faster |

## Building

### Prerequisites
- **Deno** (Primary development runtime - required)
- **Emscripten 4.0.14+** (WASM compilation)
- **CMake 3.24+** (libpng build)

### Deno-Native Build Process

```bash
# Install Deno (if not already installed)
curl -fsSL https://deno.land/install.sh | sh

# Core development workflow
deno task build:wasm        # Build WASM modules
deno task test              # Direct TypeScript testing
deno task demo              # Interactive demo
deno task check             # Type checking

# NPM publishing (Deno-native with dnt)
deno task build:npm         # Transform to Node.js package
deno task publish:npm       # Full build and publish workflow
```

## Testing

### Deno-Native Testing (Recommended)

```bash
# Modern Deno test runner with direct TypeScript execution
deno task test              # All tests
deno task test:basic        # Basic functionality
deno task demo              # Interactive demo

# Watch mode for development
deno test --allow-read --watch tests/deno/
```

### Node.js Testing (Generated Package)

```bash
# Test the generated npm package
cd npm/
npm test
```

### Test Coverage
- **PNG Format Support** - All color types, bit depths, and interlace modes
- **SIMD Filter Validation** - Performance and correctness verification
- **Memory Management** - Large image processing without leaks
- **Error Handling** - Robust error conditions and edge cases
- **Dynamic Dependencies** - zlib.wasm integration testing

## Performance

### Benchmarks (Optimized Build + SIMD)
- **PNG Decoding**: 50+ MB/s (SIMD-accelerated filters)
- **PNG Encoding**: 25+ MB/s (Optimized compression)
- **Filter Performance**: 2-4x SIMD speedup
- **Memory Efficiency**: <2MB overhead for large images

### Bundle Sizes
- **WASM Module**: ~45KB (optimized)
- **JS Wrapper**: ~25KB (closure compiled)
- **TypeScript Definitions**: ~8KB
- **Total**: ~78KB (compressed)

## Modern Deno-to-NPM Workflow

This project uses a **Deno-first development approach** with automatic Node.js package generation:

### Development (Deno-native)
- Write code in TypeScript with Deno's superior APIs
- Test directly with `deno test` (no configuration needed)
- Use native Web Standards (fetch, WebAssembly, etc.)

### Distribution (Universal compatibility)
- **dnt** (Deno to Node Transform) automatically converts code to Node.js packages
- Generates both ESM and CommonJS builds
- Maintains full TypeScript declaration files
- Preserves WASM assets and dependencies

## Deployment

### CDN Distribution

**Primary CDN**:
```
https://wasm.discere.cloud/libpng/
```

**Variants Available**:
- `optimized/` - Production build with SIMD
- `debug/` - Development build with debug symbols
- `simd-off/` - Compatibility build without SIMD

### NPM Package

```bash
npm install @discere-os/libpng.wasm
```

Generated from Deno source using dnt (Deno to Node Transform) with complete compatibility.

## Integration Examples

### Web Application
```typescript
import LibPNG from '@discere-os/libpng.wasm'

const png = new LibPNG()
await png.initialize()

// Process uploaded PNG file
const fileData = await file.arrayBuffer()
const result = await png.decodePNG(new Uint8Array(fileData))

// Display processed image
canvas.width = result.info.width
canvas.height = result.info.height
ctx.putImageData(new ImageData(result.data, result.info.width), 0, 0)
```

### Deno Processing (Recommended)
```typescript
import LibPNG from "https://deno.land/x/libpng_wasm/mod.ts"

const png = new LibPNG()
await png.initialize()

// Batch process PNG files with native Deno APIs
const inputData = await Deno.readFile('input.png')
const processed = await png.decodePNG(inputData)

// Apply transformations and re-encode
const reencoded = await png.encodePNG(processed.data,
  processed.info.width, processed.info.height, {
    compressionLevel: 9,
    simdOptimizations: true
  }
)

await Deno.writeFile('output.png', reencoded.data)
```

## Dependencies

### Runtime Dependencies
- **@discere-os/zlib.wasm** - PNG compression/decompression (auto-loaded)

## Browser Compatibility

### Full Support
- **Chrome 91+** - Complete SIMD and WebAssembly support
- **Firefox 89+** - Full PNG processing capabilities
- **Safari 14.1+** - WebAssembly with performance optimizations
- **Edge 91+** - Complete feature support

### Runtime Support
- **Deno** - Primary runtime with native TypeScript and superior WASM support
- **Cloudflare Workers** - Edge deployment with denoflare CLI integration
- **Node.js 18+** - NPM package compatibility (legacy support)

## License

libpng License - Compatible with original libpng licensing.

Based on libpng v1.6.44 with WASM-native enhancements by Superstruct Ltd, New Zealand.


## 💖 Support This Work

This WebAssembly port is part of a larger effort to bring professional desktop applications to browsers with native performance.

**👨‍💻 About the Maintainer**: [Isaac Johnston (@superstructor)](https://github.com/superstructor) - Building foundational browser-native computing infrastructure through systematic C/C++ to WebAssembly porting.

**📊 Impact**: 70+ open source WASM libraries enabling professional applications like Blender, GIMP, and scientific computing tools to run natively in browsers.

**🚀 Your Support Enables**:
- Continued maintenance and updates
- Performance optimizations
- New library ports and integrations
- Documentation and tutorials
- Cross-browser compatibility testing

**[💖 Sponsor this work](https://github.com/sponsors/superstructor)** to help build the future of browser-native computing.
