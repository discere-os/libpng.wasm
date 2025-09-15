/**
 * PNG decode/encode with STATIC zlib linking for MAIN_MODULE builds
 * Production-ready WASM implementation using direct zlib functions
 *
 * Based on libpng v1.6.44 with WASM-native optimizations
 */

#include <png.h>
#include <zlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <emscripten.h>

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

// Error message storage
static char last_error_msg[256] = {0};

// Forward declarations
static void png_static_error_handler(png_structp png_ptr, png_const_charp error_msg);
static void png_static_warning_handler(png_structp png_ptr, png_const_charp warning_msg);
static void png_static_read_callback(png_structp png, png_bytep data, size_t length);
static void png_static_write_callback(png_structp png, png_bytep data, size_t length);

/**
 * Initialize libpng.wasm with static zlib
 * Returns 1 on success, 0 on failure
 */
EMSCRIPTEN_KEEPALIVE
int png_wasm_init(void) {
    // Verify zlib is available (static linking)
    const char* version_str = zlibVersion();
    if (version_str) {
        printf("✅ Static zlib %s available for PNG processing\n", version_str);
        return 1;
    } else {
        strncpy(last_error_msg, "Static zlib not available", sizeof(last_error_msg)-1);
        return 0;
    }
}

/**
 * Decode PNG from buffer using static zlib
 */
EMSCRIPTEN_KEEPALIVE
int png_wasm_decode_buffer(
    const unsigned char* png_data,
    size_t png_size,
    unsigned char** output_buffer,
    int* width,
    int* height,
    int* channels,
    int* bit_depth,
    int* color_type
) {
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep* row_pointers = NULL;
    unsigned char* image_data = NULL;

    // Validate input
    if (!png_data || png_size == 0 || !output_buffer) {
        strncpy(last_error_msg, "Invalid input parameters", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    // Check PNG signature
    if (png_sig_cmp(png_data, 0, 8) != 0) {
        strncpy(last_error_msg, "Invalid PNG signature", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    // Initialize libpng structures
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                png_static_error_handler, png_static_warning_handler);
    if (!png) {
        strncpy(last_error_msg, "Failed to create PNG read struct", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        strncpy(last_error_msg, "Failed to create PNG info struct", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    // Set up error handling
    if (setjmp(png_jmpbuf(png))) {
        if (row_pointers) free(row_pointers);
        if (image_data) free(image_data);
        png_destroy_read_struct(&png, &info, NULL);
        // Error message already set by error handler
        return 0; // Failure (zero for JavaScript falsiness)
    }

    // Set up memory I/O
    png_read_state_t read_state = { png_data, png_size, 0 };
    png_set_read_fn(png, &read_state, png_static_read_callback);

    // Read PNG info
    png_read_info(png, info);

    int png_width = png_get_image_width(png, info);
    int png_height = png_get_image_height(png, info);
    int png_color_type = png_get_color_type(png, info);
    int png_bit_depth = png_get_bit_depth(png, info);
    int png_channels = png_get_channels(png, info);

    // Apply transformations for consistent output
    if (png_color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
        png_channels = 3;
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
        png_channels++;
    }

    if (png_bit_depth == 16) {
        png_set_strip_16(png);
        png_bit_depth = 8;
    }

    if (png_bit_depth < 8) {
        png_set_packing(png);
        png_bit_depth = 8;
    }

    // Update info after transformations
    png_read_update_info(png, info);

    // Allocate image data
    size_t row_bytes = png_get_rowbytes(png, info);
    image_data = (unsigned char*)malloc(png_height * row_bytes);
    if (!image_data) {
        png_destroy_read_struct(&png, &info, NULL);
        strncpy(last_error_msg, "Memory allocation failed for image data", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    // Allocate row pointers
    row_pointers = (png_bytep*)malloc(png_height * sizeof(png_bytep));
    if (!row_pointers) {
        free(image_data);
        png_destroy_read_struct(&png, &info, NULL);
        strncpy(last_error_msg, "Memory allocation failed for row pointers", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    // Set up row pointers
    for (int y = 0; y < png_height; y++) {
        row_pointers[y] = image_data + y * row_bytes;
    }

    // Read the image
    png_read_image(png, row_pointers);
    png_read_end(png, NULL);

    // Set output parameters
    if (width) *width = png_width;
    if (height) *height = png_height;
    if (channels) *channels = png_channels;
    if (bit_depth) *bit_depth = png_bit_depth;
    if (color_type) *color_type = png_color_type;
    *output_buffer = image_data;

    // Cleanup
    free(row_pointers);
    png_destroy_read_struct(&png, &info, NULL);

    printf("✅ PNG decoded: %dx%d, %d channels using static zlib\n", png_width, png_height, png_channels);
    return 1; // Success (non-zero for JavaScript truthiness)
}

/**
 * Encode image data to PNG buffer using static zlib
 */
EMSCRIPTEN_KEEPALIVE
int png_wasm_encode_buffer(
    const unsigned char* image_data,
    int width,
    int height,
    int channels,
    unsigned char** png_buffer,
    size_t* png_size
) {
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep* row_pointers = NULL;

    // Validate input
    if (!image_data || width <= 0 || height <= 0 || channels <= 0 || !png_buffer || !png_size) {
        strncpy(last_error_msg, "Invalid encode parameters", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    // Initialize libpng structures
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                 png_static_error_handler, png_static_warning_handler);
    if (!png) {
        strncpy(last_error_msg, "Failed to create PNG write struct", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        strncpy(last_error_msg, "Failed to create PNG info struct", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    // Set up error handling
    if (setjmp(png_jmpbuf(png))) {
        if (row_pointers) free(row_pointers);
        png_destroy_write_struct(&png, &info);
        // Error message already set by error handler
        return 0; // Failure (zero for JavaScript falsiness)
    }

    // Set up memory I/O
    png_write_state_t write_state = { NULL, 0, 0 };
    png_set_write_fn(png, &write_state, png_static_write_callback, NULL);

    // Set PNG properties
    int color_type;
    switch (channels) {
        case 1: color_type = PNG_COLOR_TYPE_GRAY; break;
        case 2: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
        case 3: color_type = PNG_COLOR_TYPE_RGB; break;
        case 4: color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
        default:
            png_destroy_write_struct(&png, &info);
            strncpy(last_error_msg, "Unsupported channel count", sizeof(last_error_msg)-1);
            return 0; // Failure (zero for JavaScript falsiness)
    }

    png_set_IHDR(png, info, width, height, 8, color_type,
                PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    // Write header
    png_write_info(png, info);

    // Set up row pointers
    row_pointers = (png_bytep*)malloc(height * sizeof(png_bytep));
    if (!row_pointers) {
        png_destroy_write_struct(&png, &info);
        strncpy(last_error_msg, "Memory allocation failed for row pointers", sizeof(last_error_msg)-1);
        return 0; // Failure (zero for JavaScript falsiness)
    }

    size_t row_bytes = width * channels;
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_bytep)(image_data + y * row_bytes);
    }

    // Write image data
    png_write_image(png, row_pointers);
    png_write_end(png, NULL);

    // Set output
    *png_buffer = write_state.buffer;
    *png_size = write_state.size;

    // Cleanup
    free(row_pointers);
    png_destroy_write_struct(&png, &info);

    printf("✅ PNG encoded: %dx%d, %d channels, %zu bytes using static zlib\n",
           width, height, channels, write_state.size);
    return 1; // Success (non-zero for JavaScript truthiness)
}

/**
 * Get SIMD support status
 */
EMSCRIPTEN_KEEPALIVE
int png_wasm_get_simd_supported(void) {
#ifdef __wasm_simd128__
    return 1;
#else
    return 0;
#endif
}

/**
 * Enable/disable SIMD optimizations
 */
EMSCRIPTEN_KEEPALIVE
void png_wasm_set_simd_enabled(int enabled) {
    // SIMD optimizations are compile-time enabled
    (void)enabled;
}

/**
 * Cleanup resources
 */
EMSCRIPTEN_KEEPALIVE
void png_wasm_cleanup(void) {
    // No dynamic resources to clean up in static build
    printf("✅ PNG cleanup completed (static zlib build)\n");
}

/**
 * Get last error message
 */
EMSCRIPTEN_KEEPALIVE
const char* png_wasm_get_last_error(void) {
    return last_error_msg;
}

/**
 * Error handler
 */
static void png_static_error_handler(png_structp png_ptr, png_const_charp error_msg) {
    // Store error message for retrieval
    strncpy(last_error_msg, error_msg, sizeof(last_error_msg) - 1);
    last_error_msg[sizeof(last_error_msg) - 1] = '\0';

    printf("PNG error: %s\n", error_msg);
    longjmp(png_jmpbuf(png_ptr), 1);
}

/**
 * Warning handler
 */
static void png_static_warning_handler(png_structp png_ptr, png_const_charp warning_msg) {
    printf("PNG warning: %s\n", warning_msg);
    (void)png_ptr;
}

/**
 * PNG read callback for memory I/O
 */
static void png_static_read_callback(png_structp png, png_bytep data, size_t length) {
    png_read_state_t* state = (png_read_state_t*)png_get_io_ptr(png);

    if (state->offset + length > state->size) {
        png_error(png, "PNG read past buffer end");
    }

    memcpy(data, state->data + state->offset, length);
    state->offset += length;
}

/**
 * PNG write callback for memory I/O
 */
static void png_static_write_callback(png_structp png, png_bytep data, size_t length) {
    png_write_state_t* state = (png_write_state_t*)png_get_io_ptr(png);

    // Grow buffer if needed
    if (state->size + length > state->capacity) {
        size_t new_capacity = (state->capacity == 0) ? 8192 : state->capacity * 2;
        while (new_capacity < state->size + length) {
            new_capacity *= 2;
        }

        state->buffer = (unsigned char*)realloc(state->buffer, new_capacity);
        if (!state->buffer) {
            png_error(png, "Memory allocation failed");
        }
        state->capacity = new_capacity;
    }

    memcpy(state->buffer + state->size, data, length);
    state->size += length;
}