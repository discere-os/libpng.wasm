/**
 * zlib_external_stub.c - External zlib.wasm stub functions
 * 
 * This file provides zlib function stubs that redirect to external zlib-side.wasm
 * Ensures NO embedded zlib implementation - pure external dependency
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <emscripten.h>

// External zlib.wasm module handle and function pointers
static void* zlib_handle = NULL;
static int zlib_loaded = 0;

// zlib function pointers (loaded from zlib-side.wasm)
static int (*external_deflateInit2_)(void* strm, int level, int method, int windowBits, int memLevel, int strategy, const char* version, int stream_size) = NULL;
static int (*external_deflate)(void* strm, int flush) = NULL;
static int (*external_deflateEnd)(void* strm) = NULL;
static int (*external_deflateReset)(void* strm) = NULL;
static int (*external_inflateInit2_)(void* strm, int windowBits, const char* version, int stream_size) = NULL;
static int (*external_inflate)(void* strm, int flush) = NULL;
static int (*external_inflateEnd)(void* strm) = NULL;
static int (*external_inflateReset)(void* strm) = NULL;
static int (*external_inflateReset2)(void* strm, int windowBits) = NULL;
static unsigned long (*external_crc32)(unsigned long crc, const unsigned char* buf, unsigned int len) = NULL;

/**
 * Load external zlib.wasm SIDE_MODULE and resolve function pointers
 */
static int load_external_zlib_functions(void) {
    if (zlib_loaded) return 0; // Already loaded
    
    printf("🔧 Loading external zlib-side.wasm...\n");
    
    // Try to load zlib-side.wasm SIDE_MODULE
    zlib_handle = dlopen("zlib-side.wasm", RTLD_NOW);
    if (!zlib_handle) {
        printf("❌ Failed to load zlib-side.wasm: %s\n", dlerror());
        return -1;
    }
    
    // Load all required zlib functions
    external_deflateInit2_ = (int(*)(void*, int, int, int, int, int, const char*, int))dlsym(zlib_handle, "deflateInit2_");
    external_deflate = (int(*)(void*, int))dlsym(zlib_handle, "deflate");
    external_deflateEnd = (int(*)(void*))dlsym(zlib_handle, "deflateEnd");
    external_deflateReset = (int(*)(void*))dlsym(zlib_handle, "deflateReset");
    external_inflateInit2_ = (int(*)(void*, int, const char*, int))dlsym(zlib_handle, "inflateInit2_");
    external_inflate = (int(*)(void*, int))dlsym(zlib_handle, "inflate");
    external_inflateEnd = (int(*)(void*))dlsym(zlib_handle, "inflateEnd");
    external_inflateReset = (int(*)(void*))dlsym(zlib_handle, "inflateReset");
    external_inflateReset2 = (int(*)(void*, int))dlsym(zlib_handle, "inflateReset2");
    external_crc32 = (unsigned long(*)(unsigned long, const unsigned char*, unsigned int))dlsym(zlib_handle, "crc32");
    
    // Verify all functions were loaded
    if (!external_deflateInit2_ || !external_deflate || !external_deflateEnd || 
        !external_deflateReset || !external_inflateInit2_ || !external_inflate ||
        !external_inflateEnd || !external_inflateReset || !external_inflateReset2 || !external_crc32) {
        printf("❌ Failed to load all zlib symbols from external module\n");
        dlclose(zlib_handle);
        zlib_handle = NULL;
        return -1;
    }
    
    zlib_loaded = 1;
    printf("✅ External zlib.wasm functions loaded successfully\n");
    return 0;
}

// Stub functions that redirect to external zlib.wasm

int deflateInit2_(void* strm, int level, int method, int windowBits, int memLevel, int strategy, const char* version, int stream_size) {
    if (load_external_zlib_functions() != 0) {
        printf("❌ External zlib not available for deflateInit2_\n");
        return -2; // Z_MEM_ERROR
    }
    return external_deflateInit2_(strm, level, method, windowBits, memLevel, strategy, version, stream_size);
}

int deflate(void* strm, int flush) {
    if (!zlib_loaded || !external_deflate) {
        printf("❌ External zlib not available for deflate\n");
        return -2; // Z_MEM_ERROR
    }
    return external_deflate(strm, flush);
}

int deflateEnd(void* strm) {
    if (!zlib_loaded || !external_deflateEnd) {
        printf("❌ External zlib not available for deflateEnd\n");
        return -2; // Z_MEM_ERROR
    }
    return external_deflateEnd(strm);
}

int deflateReset(void* strm) {
    if (!zlib_loaded || !external_deflateReset) {
        printf("❌ External zlib not available for deflateReset\n");
        return -2; // Z_MEM_ERROR
    }
    return external_deflateReset(strm);
}

int inflateInit2_(void* strm, int windowBits, const char* version, int stream_size) {
    if (load_external_zlib_functions() != 0) {
        printf("❌ External zlib not available for inflateInit2_\n");
        return -2; // Z_MEM_ERROR
    }
    return external_inflateInit2_(strm, windowBits, version, stream_size);
}

int inflate(void* strm, int flush) {
    if (!zlib_loaded || !external_inflate) {
        printf("❌ External zlib not available for inflate\n");
        return -2; // Z_MEM_ERROR
    }
    return external_inflate(strm, flush);
}

int inflateEnd(void* strm) {
    if (!zlib_loaded || !external_inflateEnd) {
        printf("❌ External zlib not available for inflateEnd\n");
        return -2; // Z_MEM_ERROR
    }
    return external_inflateEnd(strm);
}

int inflateReset(void* strm) {
    if (!zlib_loaded || !external_inflateReset) {
        printf("❌ External zlib not available for inflateReset\n");
        return -2; // Z_MEM_ERROR
    }
    return external_inflateReset(strm);
}

int inflateReset2(void* strm, int windowBits) {
    if (!zlib_loaded || !external_inflateReset2) {
        printf("❌ External zlib not available for inflateReset2\n");
        return -2; // Z_MEM_ERROR
    }
    return external_inflateReset2(strm, windowBits);
}

unsigned long crc32(unsigned long crc, const unsigned char* buf, unsigned int len) {
    if (load_external_zlib_functions() != 0) {
        printf("❌ External zlib not available for crc32\n");
        return 0;
    }
    return external_crc32(crc, buf, len);
}

/**
 * Cleanup external zlib resources
 */
EMSCRIPTEN_KEEPALIVE
void zlib_external_cleanup(void) {
    if (zlib_handle) {
        dlclose(zlib_handle);
        zlib_handle = NULL;
        zlib_loaded = 0;
    }
}