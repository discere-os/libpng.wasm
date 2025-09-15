#!/usr/bin/env -S deno bench --allow-read

/**
 * libpng.wasm Performance Benchmarks
 * Comprehensive benchmarking suite for PNG filter algorithms, compression ratios, and processing speed
 */

import LibPNG from "../src/lib/index.ts";
import { PNGColorType, PNGFilter } from "../src/lib/types.ts";

// Test data generators for different image characteristics
function generateGradientPattern(width: number, height: number, channels: number): Uint8Array {
  const data = new Uint8Array(width * height * channels);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * channels;
      data[idx] = (x * 255) / width;         // R gradient
      if (channels > 1) data[idx + 1] = (y * 255) / height; // G gradient
      if (channels > 2) data[idx + 2] = 128;                 // B constant
      if (channels > 3) data[idx + 3] = 255;                 // A opaque
    }
  }
  return data;
}

function generateNoisePattern(width: number, height: number, channels: number): Uint8Array {
  const data = new Uint8Array(width * height * channels);
  for (let i = 0; i < data.length; i++) {
    data[i] = Math.floor(Math.random() * 256);
  }
  return data;
}

function generateSolidColor(width: number, height: number, channels: number, color: number[]): Uint8Array {
  const data = new Uint8Array(width * height * channels);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * channels;
      for (let c = 0; c < channels; c++) {
        data[idx + c] = color[c] || 0;
      }
    }
  }
  return data;
}

function generateCheckerboard(width: number, height: number, channels: number, size: number = 8): Uint8Array {
  const data = new Uint8Array(width * height * channels);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * channels;
      const isWhite = Math.floor(x / size) % 2 === Math.floor(y / size) % 2;
      const value = isWhite ? 255 : 0;
      for (let c = 0; c < channels; c++) {
        data[idx + c] = c === 3 ? 255 : value; // Alpha always 255
      }
    }
  }
  return data;
}

// Benchmark configurations
const BENCHMARK_CONFIGS = [
  { width: 32, height: 32, name: "Small (32x32)" },
  { width: 128, height: 128, name: "Medium (128x128)" },
  { width: 256, height: 256, name: "Large (256x256)" },
];

const PATTERN_GENERATORS = [
  { name: "Gradient", fn: generateGradientPattern },
  { name: "Noise", fn: generateNoisePattern },
  { name: "Solid", fn: (w: number, h: number, c: number) => generateSolidColor(w, h, c, [128, 128, 128, 255]) },
  { name: "Checkerboard", fn: generateCheckerboard },
];

// Initialize libpng once for all benchmarks
let libpng: LibPNG;

Deno.bench({
  name: "Setup - Initialize libpng.wasm",
  group: "setup",
  baseline: true,
  fn: async () => {
    libpng = new LibPNG({
      simdOptimizations: false,
      preloadZlib: false // Skip for benchmarking to avoid CDN delays
    });
    await libpng.initialize();
  },
});

// Encoding benchmarks for different image sizes and patterns
for (const config of BENCHMARK_CONFIGS) {
  for (const pattern of PATTERN_GENERATORS) {
    const channels = 4; // RGBA for consistency

    Deno.bench({
      name: `Encode - ${config.name} ${pattern.name} RGBA`,
      group: "encoding",
      fn: async () => {
        const imageData = pattern.fn(config.width, config.height, channels);
        await libpng.encodePNG(imageData, config.width, config.height);
      },
    });
  }
}

// Decoding benchmarks (encode first, then decode)
const testImages = new Map<string, Uint8Array>();

Deno.bench({
  name: "Setup - Pre-encode test images for decode benchmarks",
  group: "setup",
  fn: async () => {
    for (const config of BENCHMARK_CONFIGS) {
      for (const pattern of PATTERN_GENERATORS) {
        const channels = 4;
        const imageData = pattern.fn(config.width, config.height, channels);
        const result = await libpng.encodePNG(imageData, config.width, config.height);
        const key = `${config.name}-${pattern.name}`;
        testImages.set(key, result.data);
      }
    }
  },
});

for (const config of BENCHMARK_CONFIGS) {
  for (const pattern of PATTERN_GENERATORS) {
    Deno.bench({
      name: `Decode - ${config.name} ${pattern.name} RGBA`,
      group: "decoding",
      fn: async () => {
        const key = `${config.name}-${pattern.name}`;
        const pngData = testImages.get(key)!;
        await libpng.decodePNG(pngData);
      },
    });
  }
}

// Round-trip benchmarks (encode + decode)
for (const config of BENCHMARK_CONFIGS) {
  Deno.bench({
    name: `Round-trip - ${config.name} Gradient RGBA`,
    group: "round-trip",
    fn: async () => {
      const imageData = generateGradientPattern(config.width, config.height, 4);
      const encoded = await libpng.encodePNG(imageData, config.width, config.height);
      await libpng.decodePNG(encoded.data);
    },
  });
}

// Memory efficiency benchmarks
Deno.bench({
  name: "Memory - Large image processing (512x512)",
  group: "memory",
  fn: async () => {
    const imageData = generateGradientPattern(512, 512, 4);
    const result = await libpng.encodePNG(imageData, 512, 512);
    await libpng.decodePNG(result.data);
  },
});

// Color type benchmarks
const COLOR_TYPES = [
  { channels: 1, name: "Grayscale" },
  { channels: 3, name: "RGB" },
  { channels: 4, name: "RGBA" },
];

for (const colorType of COLOR_TYPES) {
  Deno.bench({
    name: `Color Type - 128x128 ${colorType.name}`,
    group: "color-types",
    fn: async () => {
      const imageData = generateGradientPattern(128, 128, colorType.channels);
      await libpng.encodePNG(imageData, 128, 128);
    },
  });
}

// Compression efficiency benchmarks (different patterns)
const COMPRESSION_PATTERNS = [
  {
    name: "Highly Compressible (Solid Color)",
    fn: (w: number, h: number) => generateSolidColor(w, h, 4, [255, 0, 0, 255])
  },
  {
    name: "Medium Compressible (Gradient)",
    fn: (w: number, h: number) => generateGradientPattern(w, h, 4)
  },
  {
    name: "Low Compressible (Noise)",
    fn: (w: number, h: number) => generateNoisePattern(w, h, 4)
  },
];

for (const pattern of COMPRESSION_PATTERNS) {
  Deno.bench({
    name: `Compression - ${pattern.name}`,
    group: "compression",
    fn: async () => {
      const imageData = pattern.fn(128, 128);
      await libpng.encodePNG(imageData, 128, 128);
    },
  });
}

// Cleanup benchmark
Deno.bench({
  name: "Cleanup - libpng.wasm cleanup",
  group: "cleanup",
  fn: () => {
    if (libpng) {
      libpng.cleanup();
    }
  },
});