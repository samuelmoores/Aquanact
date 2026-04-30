#!/usr/bin/env bash
# Build Aquanact for WebGL / itch.io using Emscripten + vcpkg
#
# Usage (from project root):
#   bash build_web.sh
#
# Output:  build_web/index.html  (+ index.js, index.wasm, index.data)
# Upload all four files to itch.io as a ZIP.

set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SOURCE_DIR/out/build/wasm-release"
DIST_DIR="$SOURCE_DIR/build_web"
EMSCRIPTEN_TOOLCHAIN="$SOURCE_DIR/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"

echo "=== Aquanact Web Build ==="
echo "Source: $SOURCE_DIR"
echo "Build: $BUILD_DIR"
echo "Output: $DIST_DIR"

# Source emsdk environment if emcc not already on PATH
if ! command -v emcc &>/dev/null; then
    EMSDK_ENV="$SOURCE_DIR/emsdk/emsdk_env.sh"
    if [ -f "$EMSDK_ENV" ]; then
        echo "Sourcing emsdk environment..."
        # shellcheck source=/dev/null
        source "$EMSDK_ENV"
    else
        echo "ERROR: emcc not found on PATH and emsdk_env.sh not found at $EMSDK_ENV"
        echo "       Activate emsdk first: cd emsdk && ./emsdk activate latest"
        exit 1
    fi
fi

if [ -z "${VCPKG_ROOT:-}" ]; then
    echo "ERROR: VCPKG_ROOT is not set. Run from a Visual Studio Developer prompt or set it manually."
    exit 1
fi

if [ ! -f "$EMSCRIPTEN_TOOLCHAIN" ]; then
    echo "ERROR: Emscripten CMake toolchain not found at $EMSCRIPTEN_TOOLCHAIN"
    echo "       Install/activate emsdk first: cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    exit 1
fi

mkdir -p "$BUILD_DIR"

# Use vcpkg as the primary toolchain; the wasm32-emscripten triplet chainloads
# Emscripten's cmake toolchain via VCPKG_CHAINLOAD_TOOLCHAIN_FILE.
# Do NOT use emcmake here — that would set CMAKE_TOOLCHAIN_FILE to Emscripten
# and bypass vcpkg, so find_package(assimp) wouldn't work.
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$EMSCRIPTEN_TOOLCHAIN" \
    -DVCPKG_TARGET_TRIPLET=wasm32-emscripten

cmake --build "$BUILD_DIR" --parallel "$(nproc 2>/dev/null || echo 4)"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
cp "$BUILD_DIR/index.html" "$DIST_DIR/index.html"
cp "$BUILD_DIR/index.js" "$DIST_DIR/index.js"
cp "$BUILD_DIR/index.wasm" "$DIST_DIR/index.wasm"
cp "$BUILD_DIR/index.data" "$DIST_DIR/index.data"

echo ""
echo "=== Build complete ==="
echo "Files in $DIST_DIR/:"
ls -lh "$DIST_DIR"/index.html "$DIST_DIR"/index.js "$DIST_DIR"/index.wasm "$DIST_DIR"/index.data 2>/dev/null || true
echo ""
echo "To test locally:  emrun --no_browser --port 8080 $DIST_DIR/index.html"
echo "To deploy:        zip the four index.* files and upload to itch.io"
