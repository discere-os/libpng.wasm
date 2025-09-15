/**
 * zlib_external_only.c - External-only zlib declarations
 * 
 * This file provides ONLY external declarations for zlib functions
 * NO implementation - forces runtime dependency on external zlib-side.wasm
 */

// External declarations only - no implementations
// These will be resolved at runtime via dlopen() from zlib-side.wasm

extern int deflateInit2_(void* strm, int level, int method, int windowBits, int memLevel, int strategy, const char* version, int stream_size);
extern int deflate(void* strm, int flush);
extern int deflateEnd(void* strm);
extern int deflateReset(void* strm);
extern int inflateInit2_(void* strm, int windowBits, const char* version, int stream_size);
extern int inflate(void* strm, int flush);
extern int inflateEnd(void* strm);
extern int inflateReset(void* strm);
extern int inflateReset2(void* strm, int windowBits);
extern unsigned long crc32(unsigned long crc, const unsigned char* buf, unsigned int len);

// This file provides NO function implementations
// All zlib functions must be loaded from external zlib-side.wasm at runtime