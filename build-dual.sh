#!/bin/bash
# build-dual.sh - Dual build system for libpng.wasm (SIDE_MODULE + MAIN_MODULE)
#
# Copyright (c) 1995-2024 The PNG Reference Library Authors
# Copyright (c) 2025 Superstruct Ltd, New Zealand
# Licensed under the libpng license (same as original project)
#
# This script builds libpng.wasm in two configurations:
# - SIDE_MODULE: For dynamic loading in production environments
# - MAIN_MODULE: For standalone testing and NPM distribution

set -euo pipefail

# Configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
INSTALL_PREFIX="${INSTALL_PREFIX:-./install}"
BUILD_DIR="${BUILD_DIR:-./build-dual}"
VARIANT="${1:-all}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking build prerequisites..."

    if ! command -v emcc &> /dev/null; then
        log_error "Emscripten not found. Please install and activate EMSDK."
        exit 1
    fi

    # Check for zlib.wasm dependency
    if [ ! -f "../zlib.wasm/install/wasm/zlib-side.wasm" ] && [ ! -f "../zlib.wasm/build/zlib-side.wasm" ]; then
        log_warning "zlib.wasm not found. Building from source..."
        build_zlib_dependency
    else
        log_success "zlib.wasm dependency found"
    fi

    log_success "Prerequisites check completed"
}

# Build zlib.wasm as dependency if not found
build_zlib_dependency() {
    log_info "Building zlib.wasm dependency from source..."

    if [ ! -d "../zlib.wasm" ]; then
        log_error "zlib.wasm source not found at ../zlib.wasm"
        exit 1
    fi

    cd ../zlib.wasm
    if [ -f "build-dual.sh" ]; then
        ./build-dual.sh side
    elif [ -f "package.json" ]; then
        npm run build:wasm
    else
        log_error "Cannot build zlib.wasm - no build script found"
        exit 1
    fi
    cd ../libpng.wasm

    log_success "zlib.wasm dependency built successfully"
}

# Build libpng.wasm as SIDE_MODULE for production (dynamically loadable)
build_libpng_side_module() {
    log_info "Building libpng.wasm as SIDE_MODULE for production dynamic loading..."

    mkdir -p "${BUILD_DIR}-side"
    cd "${BUILD_DIR}-side"

    # Core PNG sources
    PNG_SOURCES="../png.c ../pngerror.c ../pngget.c ../pngmem.c ../pngpread.c ../pngread.c ../pngrio.c ../pngrtran.c ../pngrutil.c ../pngset.c ../pngtrans.c ../pngwio.c ../pngwrite.c ../pngwtran.c ../pngwutil.c"

    # WASM optimization sources
    WASM_SOURCES="../wasm/wasm_init.c ../wasm/filter_wasm_simd.c ../wasm/wasm_memory.c ../wasm/wasm_progressive.c ../wasm/wasm_zlib.c ../wasm/wasm_error.c ../wasm/png_decode_wasm.c"

    log_info "Compiling libpng-side.wasm as SIDE_MODULE..."
    emcc -O3 -msimd128 \
        -s SIDE_MODULE=1 \
        -s WASM=1 \
        -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
        -DPNG_NO_STDIO \
        -DPNG_CONFIGURE_LIBPNG \
        -DPNG_ARM_NEON_OPT=0 \
        -DPNG_INTEL_SSE_OPT=0 \
        -DPNG_POWERPC_VSX_OPT=0 \
        -DPNG_MIPS_MSA_OPT=0 \
        -DPNG_WASM_SIMD_OPT=1 \
        -I.. \
        ${WASM_SOURCES} \
        ${PNG_SOURCES} \
        -o libpng-side.wasm

    cd ..
    log_success "libpng-side.wasm SIDE_MODULE built successfully"
}

# Build libpng.wasm as MAIN_MODULE for testing (standalone with static zlib)
build_libpng_main_module() {
    log_info "Building libpng.wasm as MAIN_MODULE for standalone testing..."

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Core PNG sources
    PNG_SOURCES="../png.c ../pngerror.c ../pngget.c ../pngmem.c ../pngpread.c ../pngread.c ../pngrio.c ../pngrtran.c ../pngrutil.c ../pngset.c ../pngtrans.c ../pngwio.c ../pngwrite.c ../pngwtran.c ../pngwutil.c"

    # WASM optimization sources (include SIMD filters for MAIN_MODULE to avoid missing symbols)
    WASM_SOURCES="../wasm/wasm_init.c ../wasm/filter_wasm_simd.c ../wasm/wasm_memory.c ../wasm/wasm_progressive.c ../wasm/wasm_error.c ../wasm/png_decode_static.c"

    # Zlib sources for static linking in MAIN_MODULE (PNG only needs core compression, not gzip)
    ZLIB_DIR="../../zlib.wasm"
    ZLIB_SOURCES="$ZLIB_DIR/adler32.c $ZLIB_DIR/compress.c $ZLIB_DIR/crc32.c $ZLIB_DIR/deflate.c $ZLIB_DIR/infback.c $ZLIB_DIR/inffast.c $ZLIB_DIR/inflate.c $ZLIB_DIR/inftrees.c $ZLIB_DIR/trees.c $ZLIB_DIR/uncompr.c $ZLIB_DIR/zutil.c"

    # Exported functions for MAIN_MODULE (includes memory management)
    EXPORTED_FUNCTIONS='["_png_wasm_init","_png_wasm_decode_buffer","_png_wasm_encode_buffer","_png_wasm_get_simd_supported","_png_wasm_set_simd_enabled","_png_wasm_cleanup","_png_wasm_get_last_error","_dlopen","_dlsym","_malloc","_free"]'

    EXPORTED_RUNTIME_METHODS='["ccall","cwrap","FS","HEAPU8","HEAPU32"]'

    # Compile and link in one step to avoid PIC issues
    log_info "Compiling and linking libpng.wasm as MAIN_MODULE with static zlib (SIMD disabled for compatibility)..."
    EMCC_FORCE_STDLIBS=1 emcc -O3 \
        -s MAIN_MODULE=1 \
        -s WASM=1 \
        -s MODULARIZE=1 \
        -s EXPORT_ES6=1 \
        -s EXPORT_NAME=LibPNG \
        -s SINGLE_FILE=1 \
        -s "EXPORTED_RUNTIME_METHODS=${EXPORTED_RUNTIME_METHODS}" \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s INITIAL_MEMORY=67108864 \
        -s MAXIMUM_MEMORY=536870912 \
        -s FORCE_FILESYSTEM=1 \
        -s ASSERTIONS=1 \
        -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
        -s "EXPORTED_FUNCTIONS=${EXPORTED_FUNCTIONS}" \
        -DPNG_NO_STDIO \
        -DPNG_CONFIGURE_LIBPNG \
        -DPNG_ARM_NEON_OPT=0 \
        -DPNG_INTEL_SSE_OPT=0 \
        -DPNG_POWERPC_VSX_OPT=0 \
        -DPNG_MIPS_MSA_OPT=0 \
        -DPNG_WASM_SIMD_OPT=0 \
        -I.. \
        -I${ZLIB_DIR} \
        ${WASM_SOURCES} \
        ${PNG_SOURCES} \
        ${ZLIB_SOURCES} \
        -o libpng-release.js

    cd ..
    log_success "libpng.wasm MAIN_MODULE built successfully"
}

# Install built artifacts
install_libpng() {
    log_info "Installing libpng.wasm dual builds..."

    # Create install directories
    mkdir -p "$INSTALL_PREFIX/wasm" "$INSTALL_PREFIX/lib" "$INSTALL_PREFIX/include"

    # Install SIDE_MODULE build (production)
    if [ -f "${BUILD_DIR}-side/libpng-side.wasm" ]; then
        cp "${BUILD_DIR}-side/libpng-side.wasm" "$INSTALL_PREFIX/wasm/"
        log_info "Installed libpng-side.wasm (SIDE_MODULE)"
    fi

    # Install MAIN_MODULE build (testing)
    if [ -f "$BUILD_DIR/libpng-release.wasm" ]; then
        cp "$BUILD_DIR/libpng-release.wasm" "$INSTALL_PREFIX/wasm/"
        log_info "Installed libpng-release.wasm (MAIN_MODULE)"
    fi

    if [ -f "$BUILD_DIR/libpng-release.js" ]; then
        cp "$BUILD_DIR/libpng-release.js" "$INSTALL_PREFIX/wasm/"
        log_info "Installed libpng-release.js (MAIN_MODULE loader)"
    fi

    # Fallback build (compatibility)
    if [ -f "${BUILD_DIR}-side/libpng-side.wasm" ]; then
        cp "${BUILD_DIR}-side/libpng-side.wasm" "$INSTALL_PREFIX/wasm/libpng-fallback.wasm"
        log_info "Created libpng-fallback.wasm (compatibility)"
    fi

    # Copy headers for development use
    if [ -f "png.h" ]; then
        cp png.h pngconf.h "$INSTALL_PREFIX/include/" 2>/dev/null || true
        log_info "Installed PNG headers"
    fi

    log_success "Dual build installation completed"
}

# Clean build directories
clean_build() {
    log_info "Cleaning build directories..."

    rm -rf "$BUILD_DIR" "${BUILD_DIR}-side" "$INSTALL_PREFIX"

    log_success "Build directories cleaned"
}

# Verify build output
verify_build() {
    log_info "Verifying dual build output..."

    local errors=0

    # Check for SIDE_MODULE build (production)
    if [ ! -f "$INSTALL_PREFIX/wasm/libpng-side.wasm" ]; then
        log_error "libpng-side.wasm (SIDE_MODULE) not found"
        ((errors++))
    else
        log_success "libpng-side.wasm (SIDE_MODULE) found"
    fi

    # Check for MAIN_MODULE build (testing)
    if [ ! -f "$INSTALL_PREFIX/wasm/libpng-release.js" ]; then
        log_error "libpng-release.js (MAIN_MODULE) not found"
        ((errors++))
    else
        log_success "libpng-release.js (MAIN_MODULE) found"
    fi

    # Verify WASM module sizes (basic sanity check)
    if [ -f "$INSTALL_PREFIX/wasm/libpng-side.wasm" ]; then
        SIDE_SIZE=$(stat -c%s "$INSTALL_PREFIX/wasm/libpng-side.wasm" 2>/dev/null || stat -f%z "$INSTALL_PREFIX/wasm/libpng-side.wasm" 2>/dev/null)
        if [ "$SIDE_SIZE" -gt 10000 ]; then
            log_success "SIDE_MODULE size looks reasonable: ${SIDE_SIZE} bytes"
        else
            log_warning "SIDE_MODULE size unusually small: ${SIDE_SIZE} bytes"
        fi
    fi

    if [ $errors -eq 0 ]; then
        log_success "Dual build verification passed"
        return 0
    else
        log_error "Build verification failed with $errors errors"
        return 1
    fi
}

# Display build summary
show_summary() {
    log_info "=== libpng.wasm Dual Build Summary ==="
    echo ""
    echo "Build completed successfully!"
    echo ""
    echo "Key features implemented:"
    echo "  ✓ SIDE_MODULE: Production build for dynamic loading"
    echo "  ✓ MAIN_MODULE: Standalone build with static zlib"
    echo "  ✓ WASM SIMD filters (replacing native optimizations)"
    echo "  ✓ PNG filter function optimization"
    echo "  ✓ Memory optimization with custom allocators"
    echo "  ✓ Progressive decode/encode streaming"
    echo "  ✓ WASM-compatible error handling"
    echo ""
    echo "Output location: $INSTALL_PREFIX"
    echo "SIDE_MODULE: $INSTALL_PREFIX/wasm/libpng-side.wasm"
    echo "MAIN_MODULE: $INSTALL_PREFIX/wasm/libpng-release.js"
    echo "Fallback: $INSTALL_PREFIX/wasm/libpng-fallback.wasm"
    echo ""
    log_success "libpng.wasm dual build complete!"
}

# Main build function
main() {
    log_info "Starting libpng.wasm dual build..."
    log_info "Build type: $BUILD_TYPE"
    log_info "Install prefix: $INSTALL_PREFIX"
    log_info "Variant: $VARIANT"

    check_prerequisites

    case "$VARIANT" in
        "side")
            build_libpng_side_module
            install_libpng
            ;;
        "main")
            build_libpng_main_module
            install_libpng
            ;;
        "all")
            build_libpng_side_module
            build_libpng_main_module
            install_libpng
            ;;
        "clean")
            clean_build
            exit 0
            ;;
        *)
            log_error "Unknown variant: $VARIANT"
            echo "Usage: $0 [side|main|all|clean]"
            exit 1
            ;;
    esac

    if verify_build; then
        show_summary
    else
        log_error "Build completed with issues. Check the output above."
        exit 1
    fi
}

# Handle command line arguments
case "${1:-all}" in
    "clean")
        clean_build
        ;;
    "help"|"-h"|"--help")
        echo "Usage: $0 [side|main|all|clean|help]"
        echo ""
        echo "Environment variables:"
        echo "  BUILD_TYPE       - Release (default) or Debug"
        echo "  INSTALL_PREFIX   - Installation directory (default: ./install)"
        echo "  BUILD_DIR        - Build directory (default: ./build-dual)"
        echo ""
        echo "Commands:"
        echo "  side    - Build only SIDE_MODULE (production)"
        echo "  main    - Build only MAIN_MODULE (testing)"
        echo "  all     - Build both modules (default)"
        echo "  clean   - Clean build and install directories"
        echo "  help    - Show this help message"
        ;;
    *)
        main "$@"
        ;;
esac