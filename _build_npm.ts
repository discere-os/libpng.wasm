#!/usr/bin/env -S deno run --allow-all

/**
 * Deno to Node Transform (dnt) build script for @discere-os/libpng.wasm
 *
 * This script transforms our Deno-native TypeScript code to Node.js compatible
 * packages for NPM distribution while maintaining a single Deno-first codebase.
 */

import { build, emptyDir } from "https://deno.land/x/dnt@0.40.0/mod.ts";

const outDir = "./npm";

await emptyDir(outDir);

await build({
  entryPoints: ["./src/lib/index.ts"],
  outDir,
  shims: {
    // Enable Web Standard APIs that are used by our WASM loading
    crypto: true,
    blob: true,
    undici: true,
    // Custom shims for stream/web compatibility
    custom: [{
      package: { name: "stream/web" },
      globalNames: ["ReadableStream", "WritableStream", "TransformStream"]
    }]
  },
  package: {
    // Copy metadata from our deno.json
    name: "@discere-os/libpng.wasm",
    version: "1.6.44",
    description: "High-performance PNG processing with WebAssembly, SIMD filters, and TypeScript-first API",
    license: "libpng",
    repository: {
      type: "git",
      url: "git+https://github.com/discere-os/libpng.wasm.git",
    },
    bugs: {
      url: "https://github.com/discere-os/libpng.wasm/issues",
    },
    homepage: "https://github.com/discere-os/libpng.wasm#readme",
    dependencies: {},
    devDependencies: {},
    keywords: [
      "libpng",
      "png",
      "image",
      "wasm",
      "webassembly",
      "simd",
      "filters",
      "compression",
      "graphics",
      "typescript",
      "deno",
      "browser",
      "nodejs"
    ],
    engines: {
      node: ">=18.0.0"
    },
    // Maintain compatibility with existing package.json exports
    exports: {
      ".": {
        "import": "./esm/src/lib/index.js",
        "require": "./script/src/lib/index.js",
        "types": "./types/src/lib/index.d.ts"
      },
      "./types": {
        "import": "./esm/src/lib/types.js",
        "require": "./script/src/lib/types.js",
        "types": "./types/src/lib/types.d.ts"
      },
      "./side": {
        "import": "../install/wasm/libpng-side.wasm",
        "types": "./types/src/lib/types.d.ts"
      },
      "./main": {
        "import": "../install/wasm/libpng-release.js",
        "types": "./types/src/lib/types.d.ts"
      }
    }
  },
  compilerOptions: {
    lib: ["ES2022", "DOM"],
    target: "ES2022",
    skipLibCheck: true,
    experimentalDecorators: true,
    emitDecoratorMetadata: true
  },
  // Skip testing during npm build to avoid network issues
  test: false,
});

// Copy WASM files and other assets to npm package
console.log("📦 Copying WASM assets to npm package...");

try {
  // Ensure assets directory exists
  await Deno.mkdir(`${outDir}/assets`, { recursive: true });

  await Deno.copyFile("./install/wasm/libpng-side.wasm", `${outDir}/assets/libpng-side.wasm`);
  await Deno.copyFile("./install/wasm/libpng-release.wasm", `${outDir}/assets/libpng-release.wasm`);
  await Deno.copyFile("./install/wasm/libpng-release.js", `${outDir}/assets/libpng-release.js`);
  console.log("✅ WASM assets copied successfully");
} catch (error) {
  console.log("⚠️  WASM assets not found - run 'deno task build:wasm' first");
  console.log(`   ${error.message}`);
}

// Copy README and other documentation
await Deno.copyFile("./README.md", `${outDir}/README.md`);

try {
  await Deno.copyFile("./README_DENO.md", `${outDir}/README_DENO.md`);
} catch {
  console.log("ℹ️  README_DENO.md not found - skipping");
}

console.log("🚀 npm package built successfully!");
console.log("📁 Output directory:", outDir);
console.log("");
console.log("Next steps:");
console.log("  cd npm/");
console.log("  npm publish");