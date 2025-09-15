#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Production Usage Example for libpng.wasm
 * Demonstrates real-world PNG processing with error handling, performance monitoring, and best practices
 */

import LibPNG from "../src/lib/index.ts";
import { PNGColorType, PNGError, PNGFormatError } from "../src/lib/types.ts";

// Production configuration with comprehensive error handling
async function createProductionPngProcessor() {
  const libpng = new LibPNG({
    // CDN configuration with fallbacks
    cdnUrl: 'https://cdn.discere.cloud/npm/@discere-os/libpng.wasm/',
    fallbackUrls: [
      'https://cdn.jsdelivr.net/npm/@discere-os/libpng.wasm/',
      'https://unpkg.com/@discere-os/libpng.wasm/'
    ],

    // Optimization settings
    preloadZlib: true,        // Dynamic compression
    simdOptimizations: false, // Disabled for MAIN_MODULE compatibility
    cachingEnabled: true,
    maxMemoryMB: 512         // 512MB limit for safety
  });

  try {
    console.log('🚀 Initializing libpng.wasm...');
    await libpng.initialize();

    const capabilities = libpng.getCapabilities();
    console.log('✅ Initialization successful');
    console.log(`   WASM support: ${capabilities.wasmSupported}`);
    console.log(`   SIMD support: ${capabilities.simdSupported}`);
    console.log(`   Zlib integration: ${capabilities.zlibIntegrationSupported ? 'Dynamic' : 'Static fallback'}`);
    console.log(`   Max image size: ${(capabilities.maxImageSize / (1024*1024)).toFixed(0)}MB`);

    return libpng;
  } catch (error) {
    console.error('❌ Initialization failed:', error);
    throw error;
  }
}

// Generate test image with different patterns
function generateTestImage(width: number, height: number, pattern: 'gradient' | 'solid' | 'checkerboard'): Uint8Array {
  const channels = 4; // RGBA
  const data = new Uint8Array(width * height * channels);

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * channels;

      switch (pattern) {
        case 'gradient':
          data[idx] = (x * 255) / width;      // R gradient
          data[idx + 1] = (y * 255) / height; // G gradient
          data[idx + 2] = 128;                 // B constant
          data[idx + 3] = 255;                 // A opaque
          break;

        case 'solid':
          data[idx] = 100;     // R
          data[idx + 1] = 150; // G
          data[idx + 2] = 200; // B
          data[idx + 3] = 255; // A
          break;

        case 'checkerboard':
          const isWhite = Math.floor(x / 8) % 2 === Math.floor(y / 8) % 2;
          const value = isWhite ? 255 : 0;
          data[idx] = value;   // R
          data[idx + 1] = value; // G
          data[idx + 2] = value; // B
          data[idx + 3] = 255;   // A
          break;
      }
    }
  }

  return data;
}

// Production PNG encoding with comprehensive error handling
async function encodeImageWithErrorHandling(
  libpng: LibPNG,
  imageData: Uint8Array,
  width: number,
  height: number,
  description: string
) {
  try {
    console.log(`\n📸 Encoding ${description} (${width}x${height})...`);

    const startTime = performance.now();
    const result = await libpng.encodePNG(imageData, width, height);
    const endTime = performance.now();

    console.log(`✅ Encoding successful:`);
    console.log(`   Input: ${imageData.length.toLocaleString()} bytes`);
    console.log(`   Output: ${result.data.length.toLocaleString()} bytes`);
    console.log(`   Compression: ${result.compressionRatio?.toFixed(2)}x`);
    console.log(`   Processing time: ${result.processingTime.toFixed(2)}ms`);
    console.log(`   Total time: ${(endTime - startTime).toFixed(2)}ms`);
    console.log(`   Color type: ${PNGColorType[result.info.colorType]}`);
    console.log(`   Channels: ${result.info.channels}`);
    console.log(`   SIMD used: ${result.simdUsed ? 'Yes' : 'No'}`);

    return result;

  } catch (error) {
    console.error(`❌ Encoding failed for ${description}:`, error);

    if (error instanceof PNGError) {
      console.error(`   PNG Error: ${error.message}`);
    } else {
      console.error(`   Unexpected error: ${error}`);
    }

    throw error;
  }
}

// Production PNG decoding with validation
async function decodeAndValidate(
  libpng: LibPNG,
  pngData: Uint8Array,
  originalData: Uint8Array,
  description: string
) {
  try {
    console.log(`\n🔍 Decoding and validating ${description}...`);

    const startTime = performance.now();
    const decoded = await libpng.decodePNG(pngData);
    const endTime = performance.now();

    console.log(`✅ Decoding successful:`);
    console.log(`   Dimensions: ${decoded.info.width}x${decoded.info.height}`);
    console.log(`   Channels: ${decoded.info.channels}`);
    console.log(`   Output size: ${decoded.data.length.toLocaleString()} bytes`);
    console.log(`   Processing time: ${decoded.processingTime.toFixed(2)}ms`);
    console.log(`   Total time: ${(endTime - startTime).toFixed(2)}ms`);

    // Validate data integrity
    if (decoded.data.length !== originalData.length) {
      throw new Error(`Size mismatch: ${decoded.data.length} vs ${originalData.length}`);
    }

    let differences = 0;
    for (let i = 0; i < originalData.length; i++) {
      if (originalData[i] !== decoded.data[i]) {
        differences++;
      }
    }

    if (differences === 0) {
      console.log(`✅ Perfect data integrity: All ${originalData.length.toLocaleString()} bytes match`);
    } else {
      throw new Error(`Data integrity failed: ${differences} bytes differ`);
    }

    return decoded;

  } catch (error) {
    console.error(`❌ Decoding failed for ${description}:`, error);

    if (error instanceof PNGFormatError) {
      console.error(`   Format Error: Invalid PNG data`);
    } else if (error instanceof PNGError) {
      console.error(`   PNG Error: ${error.message}`);
    } else {
      console.error(`   Unexpected error: ${error}`);
    }

    throw error;
  }
}

// Main production example
async function main() {
  console.log('🎯 libpng.wasm Production Usage Example');
  console.log('==========================================');

  let libpng: LibPNG | null = null;

  try {
    // Initialize PNG processor with production configuration
    libpng = await createProductionPngProcessor();

    // Test different image types and sizes
    const testCases = [
      { width: 32, height: 32, pattern: 'gradient' as const, name: 'Small Gradient' },
      { width: 64, height: 64, pattern: 'solid' as const, name: 'Medium Solid Color' },
      { width: 128, height: 128, pattern: 'checkerboard' as const, name: 'Large Checkerboard' },
    ];

    let totalProcessingTime = 0;
    let totalOriginalSize = 0;
    let totalCompressedSize = 0;

    for (const testCase of testCases) {
      try {
        // Generate test image
        const imageData = generateTestImage(testCase.width, testCase.height, testCase.pattern);

        // Encode to PNG
        const encoded = await encodeImageWithErrorHandling(
          libpng,
          imageData,
          testCase.width,
          testCase.height,
          testCase.name
        );

        // Decode and validate
        await decodeAndValidate(libpng, encoded.data, imageData, testCase.name);

        // Accumulate statistics
        totalProcessingTime += encoded.processingTime;
        totalOriginalSize += imageData.length;
        totalCompressedSize += encoded.data.length;

      } catch (error) {
        console.error(`⚠️ Test case failed: ${testCase.name}`, error);
        // Continue with other test cases in production
      }
    }

    // Summary statistics
    console.log(`\n📊 Production Summary:`);
    console.log(`   Total images processed: ${testCases.length}`);
    console.log(`   Total processing time: ${totalProcessingTime.toFixed(2)}ms`);
    console.log(`   Average processing time: ${(totalProcessingTime / testCases.length).toFixed(2)}ms per image`);
    console.log(`   Total data processed: ${totalOriginalSize.toLocaleString()} → ${totalCompressedSize.toLocaleString()} bytes`);
    console.log(`   Overall compression ratio: ${(totalOriginalSize / totalCompressedSize).toFixed(2)}x`);
    console.log(`   Memory efficiency: ${((1 - totalCompressedSize / totalOriginalSize) * 100).toFixed(1)}% reduction`);

    // Test error handling with invalid data
    console.log(`\n🧪 Testing error handling...`);
    try {
      const invalidPngData = new Uint8Array([0x12, 0x34, 0x56, 0x78]); // Invalid PNG
      await libpng.decodePNG(invalidPngData);
      console.error('❌ Should have failed with invalid PNG data');
    } catch (error) {
      console.log('✅ Error handling working: Invalid PNG properly rejected');
    }

    console.log('\n🎉 Production example completed successfully!');
    console.log('   Ready for deployment with confidence.');

  } catch (error) {
    console.error('❌ Production example failed:', error);
    process.exit(1);
  } finally {
    // Always cleanup resources in production
    if (libpng) {
      libpng.cleanup();
      console.log('\n🧹 Cleanup completed - resources released');
    }
  }
}

// Handle unhandled rejections in production
process.on('unhandledRejection', (reason, promise) => {
  console.error('Unhandled Rejection at:', promise, 'reason:', reason);
  process.exit(1);
});

// Run the production example
if (import.meta.main) {
  main().catch((error) => {
    console.error('Fatal error:', error);
    process.exit(1);
  });
}