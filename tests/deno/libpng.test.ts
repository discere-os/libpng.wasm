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

    // Verify output size is reasonable
    const outputSize = new Uint32Array(Module.HEAPU32.buffer, outputSizePtr >> 2, 1)[0];
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

  // Minimal valid 1x1 RGB PNG (known working)
  const validPNG = new Uint8Array([
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // PNG signature
    0x00, 0x00, 0x00, 0x0D, // IHDR length
    0x49, 0x48, 0x44, 0x52, // IHDR chunk type
    0x00, 0x00, 0x00, 0x01, // Width: 1
    0x00, 0x00, 0x00, 0x01, // Height: 1
    0x08, 0x02, 0x00, 0x00, 0x00, // 8-bit RGB
    0x90, 0x77, 0x53, 0xDE, // IHDR CRC
    0x00, 0x00, 0x00, 0x0C, // IDAT length
    0x49, 0x44, 0x41, 0x54, // IDAT chunk type
    0x08, 0x99, 0x01, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, // Compressed data
    0x02, 0x00, 0x01, 0x00, // IDAT CRC
    0x00, 0x00, 0x00, 0x00, // IEND length
    0x49, 0x45, 0x4E, 0x44, // IEND chunk type
    0xAE, 0x42, 0x60, 0x82  // IEND CRC
  ]);

  // Allocate memory
  const pngDataPtr = Module._malloc(validPNG.length);
  const outputBufferPtr = Module._malloc(4);
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
      pngDataPtr, validPNG.length, outputBufferPtr,
      widthPtr, heightPtr, channelsPtr, bitDepthPtr, colorTypePtr
    );

    if (result === 1) {
      // Success case
      const width = new Uint32Array(Module.HEAPU32.buffer, widthPtr >> 2, 1)[0];
      const height = new Uint32Array(Module.HEAPU32.buffer, heightPtr >> 2, 1)[0];
      const channels = new Uint32Array(Module.HEAPU32.buffer, channelsPtr >> 2, 1)[0];

      assertEquals(width, 1, "Decoded width should be 1");
      assertEquals(height, 1, "Decoded height should be 1");
      assertEquals(channels, 3, "Decoded channels should be 3 (RGB)");
    } else {
      // If decoding fails, we need to investigate the IDAT compression issue
      throw new Error("PNG decoding failed - IDAT compression issue needs to be resolved");
    }

  } finally {
    // Cleanup
    Module._free(pngDataPtr);
    Module._free(outputBufferPtr);
    Module._free(widthPtr);
    Module._free(heightPtr);
    Module._free(channelsPtr);
    Module._free(bitDepthPtr);
    Module._free(colorTypePtr);
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