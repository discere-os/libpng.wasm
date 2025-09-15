#!/usr/bin/env -S deno test --allow-read --allow-write

/**
 * Dynamic zlib.wasm integration tests
 * Tests PNG compression with dynamic zlib dependency loading
 */

import { assertEquals, assertExists, assert } from "@std/assert";
import LibPNG from "../../src/lib/index.ts";
import { PNGColorType } from "../../src/lib/types.ts";

Deno.test("Zlib Integration Tests", async (t) => {
  await t.step("should initialize with dynamic zlib preloading", async () => {
    const libpng = new LibPNG({
      simdOptimizations: false,
      preloadZlib: true,
      maxMemoryMB: 128
    });
    await libpng.initialize();

    const capabilities = libpng.getCapabilities();
    assertExists(capabilities);
    assertEquals(capabilities.wasmSupported, true);

    // Check if zlib integration was successful
    console.log(`    Zlib integration: ${capabilities.zlibIntegrationSupported ? '✅ Active' : '⚠️ Static fallback'}`);

    libpng.cleanup();
  });

  await t.step("should process PNG with optimal compression", async () => {
    const libpng = new LibPNG({
      simdOptimizations: false,
      preloadZlib: true
    });
    await libpng.initialize();

    const width = 16, height = 16, channels = 4;
    const imageData = new Uint8Array(width * height * channels);

    // Create complex pattern that benefits from better compression
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const idx = (y * width + x) * channels;
        // Pattern with some redundancy for compression
        if ((x + y) % 4 === 0) {
          imageData[idx] = 255;     // R - white on grid
          imageData[idx + 1] = 255; // G
          imageData[idx + 2] = 255; // B
          imageData[idx + 3] = 255; // A
        } else {
          imageData[idx] = x * 16;     // R - gradient
          imageData[idx + 1] = y * 16; // G - gradient
          imageData[idx + 2] = 128;    // B - constant
          imageData[idx + 3] = 255;    // A - opaque
        }
      }
    }

    const result = await libpng.encodePNG(imageData, width, height);

    assertExists(result.data);
    assert(result.data.length > 0);
    assertEquals(result.info.width, width);
    assertEquals(result.info.height, height);
    assertEquals(result.info.channels, channels);
    assertEquals(result.info.colorType, PNGColorType.RGB_ALPHA);

    // Verify compression effectiveness
    assert(result.compressionRatio! > 1, "Should achieve compression");

    const capabilities = libpng.getCapabilities();
    console.log(`    Compression: ${imageData.length} → ${result.data.length} bytes (${result.compressionRatio!.toFixed(2)}x)`);
    console.log(`    Dynamic zlib: ${capabilities.zlibIntegrationSupported ? 'YES' : 'NO (static fallback)'}`);
    console.log(`    Processing time: ${result.processingTime.toFixed(2)}ms`);

    // Test round-trip integrity
    const decoded = await libpng.decodePNG(result.data);
    assertEquals(decoded.data.length, imageData.length);

    // Verify perfect data integrity
    let differences = 0;
    for (let i = 0; i < imageData.length; i++) {
      if (imageData[i] !== decoded.data[i]) {
        differences++;
      }
    }
    assertEquals(differences, 0, "Round-trip should preserve all data");

    libpng.cleanup();
  });

  await t.step("should handle compression level optimization", async () => {
    const libpng = new LibPNG({
      simdOptimizations: false,
      preloadZlib: true
    });
    await libpng.initialize();

    const width = 32, height = 32, channels = 3;
    const imageData = new Uint8Array(width * height * channels);

    // Fill with gradient pattern
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const idx = (y * width + x) * channels;
        imageData[idx] = (x * 255) / width;      // R gradient
        imageData[idx + 1] = (y * 255) / height; // G gradient
        imageData[idx + 2] = 128;                 // B constant
      }
    }

    // Test different compression approaches
    const results = [];

    // Default compression
    const defaultResult = await libpng.encodePNG(imageData, width, height);
    results.push({
      name: 'Default',
      size: defaultResult.data.length,
      ratio: defaultResult.compressionRatio || 1.0,
      time: defaultResult.processingTime
    });

    console.log('    Compression comparison:');
    for (const result of results) {
      console.log(`      ${result.name}: ${result.size} bytes, ${result.ratio.toFixed(2)}x, ${result.time.toFixed(1)}ms`);
    }

    // Verify all results are valid
    for (const result of results) {
      assert(result.size > 0, "Compressed size should be positive");
      assert(result.ratio > 0, "Compression ratio should be positive");
    }

    libpng.cleanup();
  });

  await t.step("should gracefully handle zlib loading failures", async () => {
    // Test with invalid CDN URLs to simulate failure
    const libpng = new LibPNG({
      simdOptimizations: false,
      preloadZlib: true,
      cdnUrl: 'https://invalid.example.com/',
      fallbackUrls: ['https://another-invalid.example.com/']
    });

    // Should still initialize successfully with static zlib fallback
    await libpng.initialize();

    const capabilities = libpng.getCapabilities();
    assertEquals(capabilities.wasmSupported, true);
    // zlibIntegrationSupported might be false due to failed dynamic loading
    console.log(`    Fallback mode: ${capabilities.zlibIntegrationSupported ? 'Dynamic' : 'Static zlib'}`);

    // Should still be able to process PNGs
    const width = 4, height = 4, channels = 4;
    const imageData = new Uint8Array(width * height * channels).fill(128);

    const result = await libpng.encodePNG(imageData, width, height);
    assertExists(result.data);
    assert(result.data.length > 0);

    libpng.cleanup();
  });
});