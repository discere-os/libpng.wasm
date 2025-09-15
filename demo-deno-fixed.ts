#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * libpng.wasm Deno Demo - Using TypeScript class like working repositories
 * Demonstrates proper WASM module loading with wasmBinary pattern
 */

import LibPNG from "./src/lib/index.ts";
import { PNGColorType } from "./src/lib/types.ts";

async function testLibPNGInitialization() {
  console.log("🖼️  libpng.wasm Deno Demo");
  console.log("========================");

  try {
    console.log("📦 Initializing libpng.wasm...");

    const libpng = new LibPNG({
      preloadZlib: false,  // Start without zlib to test basic initialization
      simdOptimizations: false,  // Disable SIMD to avoid missing symbol errors
      maxMemoryMB: 256
    });

    console.log("🔧 Loading WASM module with wasmBinary pattern...");
    await libpng.initialize();

    console.log("✅ libpng.wasm initialized successfully!");

    // Test capabilities
    const capabilities = libpng.getCapabilities();
    console.log("\n📊 PNG Processing Capabilities:");
    console.log(`   • WASM Support: ${capabilities.wasmSupported ? '✅' : '❌'}`);
    console.log(`   • SIMD Support: ${capabilities.simdSupported ? '✅' : '❌'}`);
    console.log(`   • SIMD Filters: ${capabilities.simdFiltersAvailable ? '✅' : '❌'}`);
    console.log(`   • zlib Integration: ${capabilities.zlibIntegrationSupported ? '✅' : '❌'}`);
    console.log(`   • Max Image Size: ${Math.round(capabilities.maxImageSize / (1024*1024))}MB`);
    console.log(`   • Supported Color Types: ${capabilities.supportedColorTypes.length}`);
    console.log(`   • Supported Bit Depths: ${capabilities.supportedBitDepths.join(', ')}`);

    // Create test image data (RGBA)
    const width = 8, height = 8, channels = 4;
    const imageSize = width * height * channels;
    const testImageData = new Uint8Array(imageSize);

    // Fill with gradient pattern
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const idx = (y * width + x) * channels;
        testImageData[idx] = (x / width) * 255;       // Red gradient
        testImageData[idx + 1] = (y / height) * 255;  // Green gradient
        testImageData[idx + 2] = 128;                  // Blue constant
        testImageData[idx + 3] = 255;                  // Alpha opaque
      }
    }

    console.log(`\n🎨 Created ${width}x${height} RGBA test image (${imageSize} bytes)`);

    // Test PNG encoding
    console.log("🔄 Testing PNG encoding...");
    const encodingStart = performance.now();

    const pngResult = await libpng.encodePNG(testImageData, width, height, {
      compressionLevel: 6,
      simdOptimizations: false
    });

    const encodingTime = performance.now() - encodingStart;

    console.log("✅ PNG encoding successful!");
    console.log(`   • Original: ${imageSize} bytes`);
    console.log(`   • Compressed: ${pngResult.data.length} bytes`);
    console.log(`   • Ratio: ${pngResult.compressionRatio?.toFixed(2)}x`);
    console.log(`   • Time: ${encodingTime.toFixed(2)}ms`);
    console.log(`   • SIMD Used: ${pngResult.simdUsed ? '✅' : '❌'}`);

    // Test PNG decoding
    console.log("\n🔄 Testing PNG decoding...");
    const decodingStart = performance.now();

    const decodedResult = await libpng.decodePNG(pngResult.data);

    const decodingTime = performance.now() - decodingStart;

    console.log("✅ PNG decoding successful!");
    console.log(`   • Decoded: ${decodedResult.data.length} bytes`);
    console.log(`   • Dimensions: ${decodedResult.info.width}x${decodedResult.info.height}`);
    console.log(`   • Channels: ${decodedResult.info.channels}`);
    console.log(`   • Color Type: ${decodedResult.info.colorType}`);
    console.log(`   • Time: ${decodingTime.toFixed(2)}ms`);
    console.log(`   • SIMD Used: ${decodedResult.simdUsed ? '✅' : '❌'}`);

    // Verify data integrity
    const originalSum = testImageData.reduce((sum, val) => sum + val, 0);
    const decodedSum = decodedResult.data.reduce((sum, val) => sum + val, 0);
    const integrity = originalSum === decodedSum ? "PERFECT" : "LOSSY";

    console.log(`   • Data Integrity: ${integrity} (${originalSum} vs ${decodedSum})`);

    // Cleanup
    libpng.cleanup();
    console.log("\n🧹 Resources cleaned up");

    return {
      success: true,
      encodingTime,
      decodingTime,
      compressionRatio: pngResult.compressionRatio,
      simdSupported: capabilities.simdSupported,
      originalSize: imageSize,
      compressedSize: pngResult.data.length,
      integrity
    };

  } catch (error) {
    console.error("❌ Demo failed:", error);
    return {
      success: false,
      error: error instanceof Error ? error.message : String(error)
    };
  }
}

// Main execution
if (import.meta.main) {
  const result = await testLibPNGInitialization();

  if (result.success) {
    console.log(`\n📈 Performance Summary:`);
    console.log(`   • Encoding: ${result.encodingTime?.toFixed(2)}ms`);
    console.log(`   • Decoding: ${result.decodingTime?.toFixed(2)}ms`);
    console.log(`   • Compression: ${result.compressionRatio?.toFixed(2)}x reduction`);
    console.log(`   • SIMD Acceleration: ${result.simdSupported ? 'Available' : 'Not Available'}`);
    console.log(`   • Data Integrity: ${result.integrity}`);

    console.log("\n🎯 Next Steps:");
    console.log("   1. Build WASM modules: deno task build");
    console.log("   2. Run tests: deno task test");
    console.log("   3. Enable zlib integration for better compression");
  } else {
    console.log("\n🔧 Troubleshooting:");
    console.log("   1. Run: deno task build");
    console.log("   2. Check: ls install/wasm/");
    console.log("   3. Verify: WASM files exist");
    Deno.exit(1);
  }
}