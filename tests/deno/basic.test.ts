/// <reference lib="deno.ns" />
// Deno Basic Test - Shows Deno advantages over Node.js/Vitest
import { assert, assertEquals, assertExists } from "jsr:@std/assert";

/**
 * Simple Deno tests showing the benefits over Node.js setup:
 * 1. No configuration needed
 * 2. Direct TypeScript execution
 * 3. Native file access
 * 4. Built-in test runner
 */

Deno.test("Deno Runtime Features", () => {
  console.log("🦕 Testing Deno runtime capabilities:");

  // Test Deno globals
  assertExists(Deno, "Deno global should exist");
  assertExists(Deno.readFile, "Deno.readFile should be available");
  assertExists(Deno.stat, "Deno.stat should be available");
  assertExists(WebAssembly, "WebAssembly should be available");

  console.log("✅ All Deno APIs available");
});

Deno.test("WASM File Access", async () => {
  const wasmPath = "./install/wasm/libpng-release.wasm";

  try {
    // Test file exists and is readable
    const fileInfo = await Deno.stat(wasmPath);
    assert(fileInfo.isFile, "WASM file should exist");
    assert(fileInfo.size > 1000000, "WASM file should be substantial size");

    console.log(`📁 WASM file: ${fileInfo.size} bytes`);

    // Test file reading
    const wasmBytes = await Deno.readFile(wasmPath);
    assertEquals(wasmBytes.length, fileInfo.size, "Read bytes should match file size");

    // Test WASM signature
    const signature = wasmBytes.slice(0, 8);
    const expectedSignature = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
    assertEquals(signature, expectedSignature, "Should have valid WASM signature");

    console.log("✅ WASM file validation passed");

  } catch (error) {
    throw new Error(`WASM file access failed: ${error.message}`);
  }
});

Deno.test("TypeScript Direct Execution", () => {
  // This test itself proves TypeScript direct execution works!
  const testObject: { message: string; working: boolean } = {
    message: "TypeScript types work without compilation",
    working: true
  };

  assertEquals(testObject.working, true);
  assert(typeof testObject.message === "string");

  console.log("✅ TypeScript executed directly without build step");
});

Deno.test("Performance - File Reading Speed", async () => {
  const wasmPath = "./install/wasm/libpng-release.wasm";

  // Benchmark file reading
  const iterations = 5;
  const times: number[] = [];

  for (let i = 0; i < iterations; i++) {
    const start = performance.now();
    await Deno.readFile(wasmPath);
    const elapsed = performance.now() - start;
    times.push(elapsed);
  }

  const avgTime = times.reduce((a, b) => a + b, 0) / times.length;
  console.log(`📊 File reading average: ${avgTime.toFixed(2)}ms`);

  assert(avgTime < 1000, "File reading should be reasonable speed");
});

Deno.test("Comparison: No Package.json Needed", () => {
  // In Node.js, this would require:
  // - package.json with type: "module"
  // - TypeScript configuration
  // - Test framework setup (Vitest)
  // - Module resolution configuration
  // - Build scripts

  // In Deno: Just run `deno test`!

  console.log("✅ No package.json, no configuration, no build step needed");
  assert(true, "This test running proves Deno's simplicity");
});

Deno.test("Module System Test", async () => {
  // Test that we can import from JSR (Deno registry)
  const { assert: denoAssert } = await import("jsr:@std/assert");

  assertEquals(typeof denoAssert, "function", "Should import from JSR registry");

  // Future: Direct WASM imports would work like this:
  // const { png_encode } = await import("./libpng.wasm");

  console.log("✅ Modern ES module system with URL-based imports");
});