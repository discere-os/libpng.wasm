/**
 * libpng.wasm - Comprehensive PNG library WASM wrapper
 * MAIN_MODULE build with Emscripten zlib integration
 *
 * Based on libpng v1.6.44 with WASM-native optimizations
 * Copyright (c) 2025 Superstruct Ltd
 */

#include <emscripten.h>
#include <png.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Error message storage
static char last_error_msg[256] = {0};

// I/O state structures
typedef struct {
    const unsigned char* data;
    size_t size;
    size_t offset;
} png_read_state_t;

typedef struct {
    unsigned char* buffer;
    size_t size;
    size_t capacity;
} png_write_state_t;

// Forward declarations
static void png_wasm_error_handler(png_structp png_ptr, png_const_charp error_msg);
static void png_wasm_warning_handler(png_structp png_ptr, png_const_charp warning_msg);
static void png_wasm_read_callback(png_structp png, png_bytep data, size_t length);
static void png_wasm_write_callback(png_structp png, png_bytep data, size_t length);

/**
 * Get libpng version string
 */
EMSCRIPTEN_KEEPALIVE
const char* libpng_wasm_version(void) {
  return png_get_libpng_ver(NULL);
}

/**
 * Get PNG version for compatibility
 */
EMSCRIPTEN_KEEPALIVE
const char* png_wasm_get_version(void) {
  return PNG_LIBPNG_VER_STRING;
}

/**
 * Initialize libpng.wasm module
 * Returns 1 on success, 0 on failure
 */
EMSCRIPTEN_KEEPALIVE
int png_wasm_init(void) {
    // MAIN_MODULE uses Emscripten's built-in zlib via -sUSE_ZLIB=1
    // No dynamic loading needed
    return 1;
}

/**
 * Check if SIMD optimizations are supported
 * Returns 1 if supported, 0 otherwise
 */
EMSCRIPTEN_KEEPALIVE
int png_wasm_get_simd_supported(void) {
    // SIMD filters currently disabled to prevent corruption
    // See wasm/wasm_init.c for details
    return 0;
}

/**
 * Get SIMD capabilities string
 */
EMSCRIPTEN_KEEPALIVE
const char* png_wasm_get_simd_capabilities(void) {
    return "WASM-SIMD-128";
}

/**
 * Get last error message
 */
EMSCRIPTEN_KEEPALIVE
const char* png_wasm_get_last_error(void) {
    return last_error_msg;
}

/**
 * Error handler callback
 */
static void png_wasm_error_handler(png_structp png_ptr, png_const_charp error_msg) {
    snprintf(last_error_msg, sizeof(last_error_msg), "PNG Error: %s", error_msg);
    fprintf(stderr, "%s\n", last_error_msg);
    longjmp(png_jmpbuf(png_ptr), 1);
}

/**
 * Warning handler callback
 */
static void png_wasm_warning_handler(png_structp png_ptr, png_const_charp warning_msg) {
    fprintf(stderr, "PNG Warning: %s\n", warning_msg);
    (void)png_ptr; // Suppress unused warning
}

/**
 * Read callback for decoding from memory buffer
 */
static void png_wasm_read_callback(png_structp png, png_bytep data, size_t length) {
    png_read_state_t* read_state = (png_read_state_t*)png_get_io_ptr(png);

    if (read_state->offset + length > read_state->size) {
        png_error(png, "Read past end of buffer");
        return;
    }

    memcpy(data, read_state->data + read_state->offset, length);
    read_state->offset += length;
}

/**
 * Write callback for encoding to memory buffer
 */
static void png_wasm_write_callback(png_structp png, png_bytep data, size_t length) {
    png_write_state_t* write_state = (png_write_state_t*)png_get_io_ptr(png);

    // Expand buffer if needed
    if (write_state->size + length > write_state->capacity) {
        size_t new_capacity = write_state->capacity * 2;
        if (new_capacity < write_state->size + length) {
            new_capacity = write_state->size + length;
        }

        unsigned char* new_buffer = (unsigned char*)realloc(write_state->buffer, new_capacity);
        if (!new_buffer) {
            png_error(png, "Failed to allocate memory for PNG encoding");
            return;
        }

        write_state->buffer = new_buffer;
        write_state->capacity = new_capacity;
    }

    memcpy(write_state->buffer + write_state->size, data, length);
    write_state->size += length;
}

/**
 * Decode PNG from buffer
 * Returns output buffer size on success, 0 on failure
 * Output buffer must be freed by caller using free()
 */
EMSCRIPTEN_KEEPALIVE
int png_wasm_decode_buffer(
    const unsigned char* png_data,
    size_t png_size,
    unsigned char** output_buffer,
    int* width,
    int* height,
    int* channels,
    int* bit_depth
) {
    if (!png_data || !output_buffer || !width || !height || !channels) {
        snprintf(last_error_msg, sizeof(last_error_msg), "Invalid parameters");
        return 0;
    }

    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep* row_pointers = NULL;
    int result_size = 0;

    // Verify PNG signature
    if (png_size < 8 || png_sig_cmp(png_data, 0, 8)) {
        snprintf(last_error_msg, sizeof(last_error_msg), "Invalid PNG signature");
        return 0;
    }

    // Create PNG read struct
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                      png_wasm_error_handler,
                                      png_wasm_warning_handler);
    if (!png_ptr) {
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to create PNG read struct");
        return 0;
    }

    // Create PNG info struct
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to create PNG info struct");
        return 0;
    }

    // Set up error handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        if (row_pointers) free(row_pointers);
        return 0;
    }

    // Set up custom read callback
    png_read_state_t read_state = { png_data, png_size, 0 };
    png_set_read_fn(png_ptr, &read_state, png_wasm_read_callback);

    // Read PNG info
    png_read_info(png_ptr, info_ptr);

    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);
    int bit_depth_value = png_get_bit_depth(png_ptr, info_ptr);

    if (bit_depth) *bit_depth = bit_depth_value;

    // Convert various formats to RGBA
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth_value < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    if (bit_depth_value == 16)
        png_set_strip_16(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    // Always add alpha channel
    png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    // Update info after transformations
    png_read_update_info(png_ptr, info_ptr);

    *channels = 4; // Always RGBA after transformations

    // Allocate row pointers
    row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * (*height));
    if (!row_pointers) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to allocate row pointers");
        return 0;
    }

    // Allocate output buffer
    size_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    result_size = row_bytes * (*height);
    *output_buffer = (unsigned char*)malloc(result_size);

    if (!(*output_buffer)) {
        free(row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to allocate output buffer");
        return 0;
    }

    // Set up row pointers
    for (int y = 0; y < *height; y++) {
        row_pointers[y] = (*output_buffer) + (y * row_bytes);
    }

    // Read the image
    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, info_ptr);

    // Cleanup
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return result_size;
}

/**
 * Encode image data to PNG buffer
 * Returns output buffer size on success, 0 on failure
 * Output buffer must be freed by caller using free()
 */
EMSCRIPTEN_KEEPALIVE
int png_wasm_encode_buffer(
    const unsigned char* image_data,
    int width,
    int height,
    int channels,
    unsigned char** output_buffer,
    int compression_level
) {
    if (!image_data || !output_buffer || width <= 0 || height <= 0) {
        snprintf(last_error_msg, sizeof(last_error_msg), "Invalid parameters");
        return 0;
    }

    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep* row_pointers = NULL;
    int result_size = 0;

    // Create PNG write struct
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                       png_wasm_error_handler,
                                       png_wasm_warning_handler);
    if (!png_ptr) {
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to create PNG write struct");
        return 0;
    }

    // Create PNG info struct
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to create PNG info struct");
        return 0;
    }

    // Set up error handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        if (row_pointers) free(row_pointers);
        return 0;
    }

    // Set up write state
    png_write_state_t write_state;
    write_state.capacity = width * height * channels + 1024; // Initial capacity
    write_state.size = 0;
    write_state.buffer = (unsigned char*)malloc(write_state.capacity);

    if (!write_state.buffer) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to allocate write buffer");
        return 0;
    }

    png_set_write_fn(png_ptr, &write_state, png_wasm_write_callback, NULL);

    // Set compression level (0-9)
    if (compression_level >= 0 && compression_level <= 9) {
        png_set_compression_level(png_ptr, compression_level);
    }

    // Determine color type
    int color_type;
    switch (channels) {
        case 1:
            color_type = PNG_COLOR_TYPE_GRAY;
            break;
        case 2:
            color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
            break;
        case 3:
            color_type = PNG_COLOR_TYPE_RGB;
            break;
        case 4:
            color_type = PNG_COLOR_TYPE_RGBA;
            break;
        default:
            free(write_state.buffer);
            png_destroy_write_struct(&png_ptr, &info_ptr);
            snprintf(last_error_msg, sizeof(last_error_msg), "Unsupported channel count: %d", channels);
            return 0;
    }

    // Set PNG header
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    // Allocate row pointers
    row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        free(write_state.buffer);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to allocate row pointers");
        return 0;
    }

    // Set up row pointers
    size_t row_bytes = width * channels;
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_bytep)(image_data + (y * row_bytes));
    }

    // Write the image
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);

    // Cleanup
    free(row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    // Return results
    *output_buffer = write_state.buffer;
    result_size = write_state.size;

    return result_size;
}

/**
 * Free buffer allocated by decode/encode functions
 */
EMSCRIPTEN_KEEPALIVE
void png_wasm_free_buffer(unsigned char* buffer) {
    if (buffer) {
        free(buffer);
    }
}
