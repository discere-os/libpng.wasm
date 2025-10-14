/**
 * Libpng WASM Benchmarks
 */

import LibpngWASM from "../src/lib/index.ts"

Deno.bench("libpng initialization", {
  baseline: true
}, async () => {
  const lib = new LibpngWASM()
  await lib.initialize()
})
