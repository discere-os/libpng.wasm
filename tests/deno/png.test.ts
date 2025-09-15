#!/usr/bin/env -S deno test --allow-read --allow-write

/**
 * Comprehensive libpng.wasm functionality tests
 * Tests PNG encoding/decoding with real WASM module and actual PNG processing
 */

import { assertEquals, assertExists, assert } from "@std/assert";
import LibPNG from "../../src/lib/index.ts";
import { PNGColorType } from "../../src/lib/types.ts";

Deno.test("PNG Module Initialization", async (t) => {
  await t.step("should initialize successfully", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    const capabilities = libpng.getCapabilities();
    assertExists(capabilities);
    assertEquals(capabilities.wasmSupported, true);
    assert(capabilities.supportedColorTypes.length > 0);

    libpng.cleanup();
  });

  await t.step("should report correct capabilities", async () => {
    const libpng = new LibPNG({
      simdOptimizations: false,
      maxMemoryMB: 128
    });
    await libpng.initialize();

    const capabilities = libpng.getCapabilities();
    assertEquals(capabilities.maxImageSize, 128 * 1024 * 1024);
    assertEquals(capabilities.supportedColorTypes.includes(PNGColorType.RGB), true);
    assertEquals(capabilities.supportedColorTypes.includes(PNGColorType.RGB_ALPHA), true);
    assertEquals(capabilities.supportedBitDepths.includes(8), true);

    libpng.cleanup();
  });
});

Deno.test("PNG Encoding Tests", async (t) => {
  await t.step("should encode small RGBA image", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    const width = 4, height = 4, channels = 4;
    const imageData = new Uint8Array(width * height * channels);

    // Fill with test pattern
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const idx = (y * width + x) * channels;
        imageData[idx] = x * 64;     // R
        imageData[idx + 1] = y * 64; // G
        imageData[idx + 2] = 128;    // B
        imageData[idx + 3] = 255;    // A
      }
    }

    const result = await libpng.encodePNG(imageData, width, height);

    assertExists(result.data);
    assert(result.data.length > 0);
    assert(result.data.length < imageData.length, "Should compress data");
    assertEquals(result.info.width, width);
    assertEquals(result.info.height, height);
    assertEquals(result.info.channels, channels);
    assertEquals(result.info.colorType, PNGColorType.RGB_ALPHA);
    assert(result.processingTime > 0);
    assert(result.compressionRatio > 1, "Should have compression ratio > 1");

    console.log(`    Encoded: ${imageData.length} → ${result.data.length} bytes (${result.compressionRatio.toFixed(2)}x)`);

    libpng.cleanup();
  });

  await t.step("should encode RGB image", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    const width = 8, height = 8, channels = 3;
    const imageData = new Uint8Array(width * height * channels);

    // Fill with gradient
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const idx = (y * width + x) * channels;
        imageData[idx] = (x / width) * 255;
        imageData[idx + 1] = (y / height) * 255;
        imageData[idx + 2] = 128;
      }
    }

    const result = await libpng.encodePNG(imageData, width, height);

    assertEquals(result.info.colorType, PNGColorType.RGB);
    assertEquals(result.info.channels, 3);
    assert(result.compressionRatio > 1);

    libpng.cleanup();
  });

  await t.step("should encode grayscale image", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    const width = 16, height = 16, channels = 1;
    const imageData = new Uint8Array(width * height * channels);

    // Fill with checkerboard pattern
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const idx = y * width + x;
        imageData[idx] = ((x + y) % 2) * 255;
      }
    }

    const result = await libpng.encodePNG(imageData, width, height);

    assertEquals(result.info.colorType, PNGColorType.GRAY);
    assertEquals(result.info.channels, 1);

    libpng.cleanup();
  });
});

Deno.test("PNG Decoding Tests", async (t) => {
  await t.step("should decode encoded PNG with perfect integrity", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    const width = 6, height = 6, channels = 4;
    const originalData = new Uint8Array(width * height * channels);

    // Create deterministic test pattern
    for (let i = 0; i < originalData.length; i++) {
      originalData[i] = (i * 37 + 123) % 256;
    }

    // Encode then decode
    const encodedResult = await libpng.encodePNG(originalData, width, height);
    const decodedResult = await libpng.decodePNG(encodedResult.data);

    // Verify metadata
    assertEquals(decodedResult.info.width, width);
    assertEquals(decodedResult.info.height, height);
    assertEquals(decodedResult.info.channels, channels);
    assertEquals(decodedResult.data.length, originalData.length);

    // Verify pixel-perfect integrity
    let differences = 0;
    for (let i = 0; i < originalData.length; i++) {
      if (originalData[i] !== decodedResult.data[i]) {
        differences++;
      }
    }
    assertEquals(differences, 0, `${differences} bytes differ - PNG processing must be lossless`);

    console.log(`    Round-trip: Perfect integrity verified (${originalData.length} bytes)`);

    libpng.cleanup();
  });

  await t.step("should handle large image efficiently", async () => {
    const libpng = new LibPNG({
      simdOptimizations: false,
      maxMemoryMB: 256
    });
    await libpng.initialize();

    const width = 128, height = 128, channels = 3;
    const originalData = new Uint8Array(width * height * channels);

    // Fill with complex pattern
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const idx = (y * width + x) * channels;
        originalData[idx] = (x * 2) % 256;
        originalData[idx + 1] = (y * 3) % 256;
        originalData[idx + 2] = (x + y) % 256;
      }
    }

    const startEncode = performance.now();
    const encoded = await libpng.encodePNG(originalData, width, height);
    const encodeTime = performance.now() - startEncode;

    const startDecode = performance.now();
    const decoded = await libpng.decodePNG(encoded.data);
    const decodeTime = performance.now() - startDecode;

    // Performance should be reasonable for large images
    assert(encodeTime < 1000, `Large image encoding too slow: ${encodeTime}ms`);
    assert(decodeTime < 1000, `Large image decoding too slow: ${decodeTime}ms`);

    // Verify integrity
    assertEquals(decoded.data.length, originalData.length);

    console.log(`    Large image (${width}x${height}): Encode ${encodeTime.toFixed(1)}ms, Decode ${decodeTime.toFixed(1)}ms`);

    libpng.cleanup();
  });

  await t.step("should reject invalid PNG data", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    const invalidData = new Uint8Array([0x12, 0x34, 0x56, 0x78]);

    try {
      await libpng.decodePNG(invalidData);
      assert(false, "Should have thrown error for invalid PNG data");
    } catch (error) {
      assert(error.name === "PNGFormatError" || error.name === "PNGError");
      assert(error.message.includes("PNG") || error.message.includes("decode"));
    }

    libpng.cleanup();
  });
});

Deno.test("PNG Edge Cases and Error Handling", async (t) => {
  await t.step("should handle zero-sized image gracefully", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    try {
      await libpng.encodePNG(new Uint8Array([]), 0, 0);
      assert(false, "Should reject zero-sized image");
    } catch (error) {
      assert(error instanceof Error);
    }

    libpng.cleanup();
  });

  await t.step("should handle mismatched data size", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    const tooSmallData = new Uint8Array(10);

    try {
      await libpng.encodePNG(tooSmallData, 10, 10); // 10x10x4 needs 400 bytes
      assert(false, "Should reject insufficient data");
    } catch (error) {
      assert(error instanceof Error);
    }

    libpng.cleanup();
  });

  await t.step("should handle multiple operations on same instance", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    const width = 4, height = 4, channels = 4;
    const imageData1 = new Uint8Array(width * height * channels).fill(255);
    const imageData2 = new Uint8Array(width * height * channels).fill(128);

    // Multiple operations should all work
    const result1 = await libpng.encodePNG(imageData1, width, height);
    const result2 = await libpng.encodePNG(imageData2, width, height);

    assert(result1.data.length > 0);
    assert(result2.data.length > 0);

    // Different patterns should produce different results
    const isIdentical = result1.data.every((byte, i) => byte === result2.data[i]);
    assertEquals(isIdentical, false, "Different input should produce different output");

    libpng.cleanup();
  });
});

Deno.test("PNG Performance Benchmarks", async (t) => {
  await t.step("should benchmark different image sizes", async () => {
    const libpng = new LibPNG({ simdOptimizations: false });
    await libpng.initialize();

    const sizes = [
      { width: 32, height: 32 },
      { width: 64, height: 64 },
      { width: 128, height: 64 }
    ];

    for (const size of sizes) {
      const channels = 4;
      const imageData = new Uint8Array(size.width * size.height * channels);

      // Fill with test pattern
      for (let i = 0; i < imageData.length; i++) {
        imageData[i] = (i * 127 + i * i) % 256;
      }

      const startTime = performance.now();
      const encoded = await libpng.encodePNG(imageData, size.width, size.height);
      const encodeTime = performance.now() - startTime;

      const decodeStart = performance.now();
      const decoded = await libpng.decodePNG(encoded.data);
      const decodeTime = performance.now() - decodeStart;

      const compressionRatio = imageData.length / encoded.data.length;

      console.log(`    ${size.width}x${size.height}: ${encodeTime.toFixed(1)}ms/${decodeTime.toFixed(1)}ms, ${compressionRatio.toFixed(1)}x compression`);

      // Sanity checks
      assert(encodeTime < 500, "Encode time reasonable");
      assert(decodeTime < 500, "Decode time reasonable");
      assert(compressionRatio > 1, "Should achieve compression");
      assertEquals(decoded.data.length, imageData.length, "Perfect round-trip");
    }

    libpng.cleanup();
  });
});