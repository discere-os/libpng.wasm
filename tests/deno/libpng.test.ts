// Deno Test Example - Much simpler than current Vitest setup
import { assert, assertEquals, assertExists } from "jsr:@std/assert";

/**
 * Deno-native libpng.wasm tests
 * No configuration needed - just run: deno test --allow-read
 */

// Load libpng MAIN_MODULE with full Emscripten runtime - Deno compatible approach
async function loadLibPNG() {
  try {
    // Import the Emscripten-generated ES6 module with the proper export name
    const LibPNGInit = await import("../../install/wasm/libpng-release.js");

    // Initialize the module following the FreeType-wasm pattern
    const LibPNG = await LibPNGInit.default();

    return LibPNG;
  } catch (error) {
    throw new Error(`Failed to load WASM module: ${error.message}`);
  }
}

// Test suite with built-in Deno test runner
Deno.test("LibPNG WASM Module Loading", async () => {
  const Module = await loadLibPNG();

  // These functions MUST exist for the library to work
  assertExists(Module._png_wasm_init, "png_wasm_init function should exist");
  assertExists(Module._png_wasm_encode_buffer, "png_wasm_encode_buffer function should exist");
  assertExists(Module._png_wasm_decode_buffer, "png_wasm_decode_buffer function should exist");
  assertExists(Module._malloc, "malloc function should exist");
  assertExists(Module._free, "free function should exist");

  console.log("✅ All expected WASM functions found in module");
});

Deno.test("PNG Initialization", async () => {
  const Module = await loadLibPNG();

  const result = Module._png_wasm_init();
  assertEquals(result, 1, "PNG initialization should return 1 for success");

  console.log("✅ PNG initialization successful");
});

Deno.test("Memory Management", async () => {
  const Module = await loadLibPNG();

  // Test memory allocation
  const size = 1024;
  const ptr = Module._malloc(size);
  assert(ptr > 0, "malloc should return valid pointer");

  // Verify memory is accessible
  const view = new Uint8Array(Module.HEAPU8.buffer, ptr, size);
  view[0] = 42;
  assertEquals(view[0], 42, "Memory should be writable and readable");

  // Clean up
  Module._free(ptr);

  console.log("✅ Memory management functional");
});

Deno.test("PNG Encoding - Basic RGB", async () => {
  const Module = await loadLibPNG();


  // Initialize PNG
  assertEquals(Module._png_wasm_init(), 1);

  // Create test image (2x2 RGB)
  const width = 2, height = 2, channels = 3;
  const imageSize = width * height * channels;
  const imageData = new Uint8Array([
    255, 0, 0,   // Red pixel
    0, 255, 0,   // Green pixel
    0, 0, 255,   // Blue pixel
    255, 255, 0  // Yellow pixel
  ]);

  // Allocate memory
  const imagePtr = Module._malloc(imageSize);
  const outputBufferPtr = Module._malloc(4); // png_bytepp
  const outputSizePtr = Module._malloc(4);   // size_t*

  try {
    // Copy image data to WASM memory
    const imageView = new Uint8Array(Module.HEAPU8.buffer, imagePtr, imageSize);
    imageView.set(imageData);

    // Encode PNG
    const result = Module._png_wasm_encode_buffer(
      imagePtr, width, height, channels,
      outputBufferPtr, outputSizePtr
    );

    assertEquals(result, 1, "PNG encoding should succeed");

    // Verify output size is reasonable (fix memory access)
    const outputSize = Module.HEAPU32[outputSizePtr >> 2];
    assert(outputSize > 50, `PNG output should be reasonable size, got ${outputSize}`);
    assert(outputSize < 1000, `PNG output should not be too large, got ${outputSize}`);

  } finally {
    // Cleanup
    Module._free(imagePtr);
    Module._free(outputBufferPtr);
    Module._free(outputSizePtr);
  }
});

Deno.test("PNG Decoding - Valid PNG data", async () => {
  const Module = await loadLibPNG();

  if (!Module) {
    console.log("⚠️  Skipping PNG decoding test - WASM module loading failed");
    assert(true, "Test skipped - WASM module loading limitation");
    return;
  }

  // Initialize PNG
  assertEquals(Module._png_wasm_init(), 1);

  // Instead of using hardcoded PNG with potentially invalid compression data,
  // create a valid PNG by encoding first, then decoding it
  const testImageData = new Uint8Array([255, 0, 0]); // 1x1 red pixel (RGB)
  const width = 1, height = 1, channels = 3;

  // Allocate memory for encoding
  const imagePtr = Module._malloc(testImageData.length);
  const outputBufferPtr = Module._malloc(4);
  const outputSizePtr = Module._malloc(4);

  try {
    // Copy test data
    const imageView = new Uint8Array(Module.HEAPU8.buffer, imagePtr, testImageData.length);
    imageView.set(testImageData);

    // Encode to create valid PNG
    const encodeResult = Module._png_wasm_encode_buffer(
      imagePtr, width, height, channels,
      outputBufferPtr, outputSizePtr
    );

    assertEquals(encodeResult, 1, "PNG encoding should succeed");

    // Get encoded PNG
    const pngSize = Module.HEAPU32[outputSizePtr >> 2];
    const encodedPngDataPtr = Module.HEAPU32[outputBufferPtr >> 2];
    const validPNG = new Uint8Array(pngSize);
    validPNG.set(Module.HEAPU8.subarray(encodedPngDataPtr, encodedPngDataPtr + pngSize));

    // Free encoding memory
    Module._free(encodedPngDataPtr);

    // Now allocate memory for decoding
    const pngDataPtr = Module._malloc(validPNG.length);
    const decodeOutputBufferPtr = Module._malloc(4);
    const widthPtr = Module._malloc(4);
    const heightPtr = Module._malloc(4);
    const channelsPtr = Module._malloc(4);
    const bitDepthPtr = Module._malloc(4);
    const colorTypePtr = Module._malloc(4);

  try {
    // Copy PNG data to WASM memory
    const pngView = new Uint8Array(Module.HEAPU8.buffer, pngDataPtr, validPNG.length);
    pngView.set(validPNG);

    // Decode PNG
    const result = Module._png_wasm_decode_buffer(
      pngDataPtr, validPNG.length, decodeOutputBufferPtr,
      widthPtr, heightPtr, channelsPtr, bitDepthPtr, colorTypePtr
    );

    if (result === 1) {
      // Success case - fix memory access patterns
      const decodedWidth = Module.HEAPU32[widthPtr >> 2];
      const decodedHeight = Module.HEAPU32[heightPtr >> 2];
      const decodedChannels = Module.HEAPU32[channelsPtr >> 2];

      assertEquals(decodedWidth, 1, "Decoded width should be 1");
      assertEquals(decodedHeight, 1, "Decoded height should be 1");
      assertEquals(decodedChannels, 3, "Decoded channels should be 3 (RGB)");
    } else {
      // If decoding fails, we need to investigate the IDAT compression issue
      throw new Error("PNG decoding failed - IDAT compression issue needs to be resolved");
    }

  } finally {
    // Cleanup decoding memory
    Module._free(pngDataPtr);
    Module._free(decodeOutputBufferPtr);
    Module._free(widthPtr);
    Module._free(heightPtr);
    Module._free(channelsPtr);
    Module._free(bitDepthPtr);
    Module._free(colorTypePtr);
  }

  } finally {
    // Cleanup encoding memory
    Module._free(imagePtr);
    Module._free(outputBufferPtr);
    Module._free(outputSizePtr);
  }
});

Deno.test("Performance - Memory allocation speed", async () => {
  const Module = await loadLibPNG();

  if (!Module) {
    console.log("⚠️  Skipping performance test - WASM module loading failed");
    assert(true, "Test skipped - WASM module loading limitation");
    return;
  }

  // Benchmark memory allocation
  const iterations = 1000;
  const size = 1024;

  const start = performance.now();

  for (let i = 0; i < iterations; i++) {
    const ptr = Module._malloc(size);
    Module._free(ptr);
  }

  const elapsed = performance.now() - start;
  const avgTime = elapsed / iterations;

  console.log(`Memory allocation average: ${avgTime.toFixed(3)}ms per allocation`);
  assert(avgTime < 1, "Memory allocation should be fast (< 1ms average)");
});

// Deno Runtime Features Test
Deno.test("Deno runtime capabilities", () => {
  console.log("✅ Deno runtime features available:");
  console.log("   - Native TypeScript execution");
  console.log("   - Built-in test runner");
  console.log("   - File system APIs");
  console.log("   - WebAssembly support");

  // Test Deno-specific APIs
  assert(typeof Deno !== 'undefined', "Deno global should be available");
  assert(typeof Deno.readFile === 'function', "Deno.readFile should be available");
  assert(typeof WebAssembly !== 'undefined', "WebAssembly should be available");
});