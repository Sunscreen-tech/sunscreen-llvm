#!/bin/bash
set -e

# Modified by Sunscreen under the AGPLv3 license; see the README at the
# repository root for more information

# Packaging script for parasol-compiler releases
# Creates a tarball with binaries in bin/ directory

# Detect platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
else
    echo "Error: Unsupported platform $OSTYPE"
    exit 1
fi

# Detect architecture
ARCH=$(uname -m)
if [[ "$ARCH" == "arm64" || "$ARCH" == "aarch64" ]]; then
    ARCH="aarch64"
elif [[ "$ARCH" == "x86_64" ]]; then
    ARCH="x86-64"
else
    echo "Error: Unsupported architecture $ARCH"
    exit 1
fi

# Get date in YYYY-MM-DD format
DATE=$(date +%Y-%m-%d)

# Define paths
BUILD_BIN="build/bin"
STAGING_DIR="release-staging"
TARBALL_NAME="parasol-compiler-${PLATFORM}-${ARCH}-${DATE}.tar.gz"

# Check if build directory exists, if not build in Release mode
if [ ! -d "build" ]; then
    echo "Build directory not found. Building LLVM in Release mode..."
    ./compile-llvm.sh all Release
    if [ $? -ne 0 ]; then
        echo "Error: Build failed"
        exit 1
    fi
fi

# Check if build/bin exists
if [ ! -d "$BUILD_BIN" ]; then
    echo "Error: $BUILD_BIN directory not found after build attempt"
    exit 1
fi

# List of binaries to package
BINARIES=(
    "clang"
    "ld.lld"
    "llvm-objdump"
    "llvm-readelf"
)

# Create staging directory structure
echo "Creating staging directory..."
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR/bin"
mkdir -p "$STAGING_DIR/lib/clang/18/include"

# Copy binaries
echo "Copying binaries..."
for binary in "${BINARIES[@]}"; do
    # Handle clang which might be a symlink to clang-18
    if [ "$binary" == "clang" ]; then
        if [ -f "$BUILD_BIN/clang-18" ]; then
            cp "$BUILD_BIN/clang-18" "$STAGING_DIR/bin/clang"
            echo "  ✓ clang (from clang-18)"
        elif [ -f "$BUILD_BIN/clang" ]; then
            cp "$BUILD_BIN/clang" "$STAGING_DIR/bin/clang"
            echo "  ✓ clang"
        else
            echo "  ✗ clang not found"
            exit 1
        fi
    else
        if [ -f "$BUILD_BIN/$binary" ]; then
            cp "$BUILD_BIN/$binary" "$STAGING_DIR/bin/$binary"
            echo "  ✓ $binary"
        else
            echo "  ✗ $binary not found"
            exit 1
        fi
    fi
done

# Copy clang builtin headers (parasol.h)
echo "Copying clang builtin headers..."
if [ -f "build/lib/clang/18/include/parasol.h" ]; then
    cp "build/lib/clang/18/include/parasol.h" "$STAGING_DIR/lib/clang/18/include/"
    echo "  ✓ parasol.h"
else
    echo "  ✗ parasol.h not found in build/lib/clang/18/include/"
    exit 1
fi

# Create tarball
echo "Creating tarball..."
cd "$STAGING_DIR"
tar -czf "../$TARBALL_NAME" bin lib
cd ..

# Cleanup
rm -rf "$STAGING_DIR"

echo ""
echo "✓ Package created successfully: $TARBALL_NAME"
echo "  Platform: $PLATFORM"
echo "  Architecture: $ARCH"
echo "  Date: $DATE"
