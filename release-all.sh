#!/usr/bin/env bash
set -euo pipefail

# Multi-architecture release builder for sunscreen-llvm
# Builds macOS (native) and Linux (Docker) tarballs, updates sunscreen-llvm.nix
#
# Usage: ./release-all.sh [VERSION] [OPTIONS]
#   VERSION: Release version (default: today's date YYYY.MM.DD)
#   --dry-run: Show what would be done without executing builds
#   --skip-macos: Skip macOS build
#   --skip-linux-arm: Skip Linux aarch64 build
#   --skip-linux-x64: Skip Linux x86-64 build

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/releases"
SOURCE_TARBALL="sunscreen-llvm-source.tar.gz"
DRY_RUN=false
BUILD_MACOS=true
BUILD_LINUX_ARM=true
BUILD_LINUX_X64=true

# Parse arguments
VERSION=""
for arg in "$@"; do
    case "$arg" in
        --dry-run)
            DRY_RUN=true
            ;;
        --skip-macos)
            BUILD_MACOS=false
            ;;
        --skip-linux-arm)
            BUILD_LINUX_ARM=false
            ;;
        --skip-linux-x64)
            BUILD_LINUX_X64=false
            ;;
        *)
            VERSION="$arg"
            ;;
    esac
done

# Default version: today's date
VERSION="${VERSION:-$(date +"%Y.%m.%d")}"

# Validate version format
if ! [[ "${VERSION}" =~ ^[0-9]{4}\.[0-9]{2}\.[0-9]{2}$ ]]; then
    echo "Error: VERSION must be in YYYY.MM.DD format, got: ${VERSION}"
    exit 1
fi

if [[ "$DRY_RUN" == "true" ]]; then
    echo "[DRY RUN MODE]"
fi
echo "Building release version: ${VERSION}"

# Convert version for filename (periods to hyphens)
FILE_VERSION="${VERSION//./-}"

# Pre-flight checks
echo "Running pre-flight checks..."

# Check git status
if [[ "$DRY_RUN" == "false" ]]; then
    if [[ -n $(git status --porcelain) ]]; then
        echo "Error: Git working directory is not clean"
        echo "Commit or stash changes before creating release"
        exit 1
    fi
else
    if [[ -n $(git status --porcelain) ]]; then
        echo "Warning: Git working directory is not clean (ignored in dry-run)"
    fi
fi

# Check Docker
if ! command -v docker &> /dev/null; then
    echo "Error: Docker not found. Install Docker Desktop."
    exit 1
fi

# Check Docker buildx
if ! docker buildx version &> /dev/null; then
    echo "Error: Docker buildx not available"
    exit 1
fi

# Check Docker memory allocation (require at least 15GB for QEMU builds)
if [[ "$BUILD_LINUX_ARM" == "true" || "$BUILD_LINUX_X64" == "true" ]]; then
    DOCKER_MEM_BYTES=$(docker info --format '{{.MemTotal}}')
    DOCKER_MEM_GB=$((DOCKER_MEM_BYTES / 1024 / 1024 / 1024))
    if [[ "$DOCKER_MEM_GB" -lt 15 ]]; then
        echo "Error: Docker has only ${DOCKER_MEM_GB}GB allocated, but Linux builds require at least 15GB"
        echo "Increase memory in Docker Desktop settings (Settings > Resources > Memory)"
        exit 1
    fi
    echo "Docker memory: ${DOCKER_MEM_GB}GB (sufficient for cross-compilation)"
fi

# Check nix-hash for hash conversion
if ! command -v nix-hash &> /dev/null; then
    echo "Error: nix-hash not found. Install Nix."
    exit 1
fi

echo "Pre-flight checks passed"

# Create output directory
if [[ "$DRY_RUN" == "false" ]]; then
    mkdir -p "${OUTPUT_DIR}"
fi

# Create git archive of HEAD
echo "Creating clean source archive from git HEAD..."
if [[ "$DRY_RUN" == "false" ]]; then
    git archive --format=tar.gz --prefix=sunscreen-llvm/ HEAD > "${OUTPUT_DIR}/${SOURCE_TARBALL}"
fi

# Expected tarball names
MACOS_TARBALL="parasol-compiler-macos-aarch64-${FILE_VERSION}.tar.gz"
LINUX_ARM_TARBALL="parasol-compiler-linux-aarch64-${FILE_VERSION}.tar.gz"
LINUX_X64_TARBALL="parasol-compiler-linux-x86-64-${FILE_VERSION}.tar.gz"

# Build macOS native
if [[ "$BUILD_MACOS" == "true" ]]; then
    echo ""
    echo "Building macOS aarch64 (native)..."
    if [[ "$DRY_RUN" == "false" ]]; then
        # Force clean build for releases
        if [[ -d "${SCRIPT_DIR}/build" ]]; then
            echo "Removing existing build directory for clean release build..."
            rm -rf "${SCRIPT_DIR}/build"
        fi

        ./compile-llvm.sh all Release
        ./package-release.sh "${FILE_VERSION}"

        # Move macOS tarball to releases (validate it exists first)
        if [[ ! -f "parasol-compiler-macos-aarch64-${FILE_VERSION}.tar.gz" ]]; then
            echo "Error: macOS tarball not created by package-release.sh"
            ls -la parasol-compiler-*.tar.gz 2>/dev/null || echo "No tarballs found"
            exit 1
        fi
        mv "parasol-compiler-macos-aarch64-${FILE_VERSION}.tar.gz" "${OUTPUT_DIR}/${MACOS_TARBALL}"
        echo "macOS build complete: ${MACOS_TARBALL}"
    else
        echo "[dry-run] Would build macOS aarch64 natively"
    fi
else
    echo ""
    echo "Skipping macOS build (--skip-macos)"
fi

# Set up Docker buildx with QEMU (only if building Linux)
if [[ "$BUILD_LINUX_ARM" == "true" || "$BUILD_LINUX_X64" == "true" ]]; then
    echo ""
    echo "Setting up Docker buildx for multi-architecture..."
    if [[ "$DRY_RUN" == "false" ]]; then
        # Check if builder exists, create if not
        if ! docker buildx inspect sunscreen-builder &>/dev/null; then
            echo "Creating new buildx builder..."
            docker buildx create --name sunscreen-builder --bootstrap
        fi
        docker buildx use sunscreen-builder
    fi
fi

# Build Linux aarch64
if [[ "$BUILD_LINUX_ARM" == "true" ]]; then
    echo ""
    echo "Building Linux aarch64 (Docker, may take tens of minutes)..."
    if [[ "$DRY_RUN" == "false" ]]; then
        cp "${OUTPUT_DIR}/${SOURCE_TARBALL}" "${SCRIPT_DIR}/${SOURCE_TARBALL}"

        docker buildx build \
            --platform linux/arm64 \
            --file Dockerfile.release \
            --tag sunscreen-llvm-builder:arm64 \
            --load \
            .

        # Extract tarball from container
        CONTAINER_ID=$(docker create sunscreen-llvm-builder:arm64)
        docker cp "${CONTAINER_ID}:/output/." "${OUTPUT_DIR}/"
        docker rm "${CONTAINER_ID}"

        # Validate and rename to expected filename
        EXTRACTED_TARBALL=$(find "${OUTPUT_DIR}" -name "parasol-compiler-linux-aarch64-*.tar.gz" -print -quit)
        if [[ -z "${EXTRACTED_TARBALL}" ]]; then
            echo "Error: No Linux aarch64 tarball found in Docker output"
            echo "Contents of ${OUTPUT_DIR}:"
            ls -la "${OUTPUT_DIR}"
            exit 1
        fi
        mv "${EXTRACTED_TARBALL}" "${OUTPUT_DIR}/${LINUX_ARM_TARBALL}"
        echo "Linux aarch64 build complete: ${LINUX_ARM_TARBALL}"
    else
        echo "[dry-run] Would build Linux aarch64 in Docker (arm64)"
    fi
else
    echo ""
    echo "Skipping Linux aarch64 build (--skip-linux-arm)"
fi

# Build Linux x86-64
if [[ "$BUILD_LINUX_X64" == "true" ]]; then
    echo ""
    echo "Building Linux x86-64 (Docker, may take tens of minutes)..."
    if [[ "$DRY_RUN" == "false" ]]; then
        docker buildx build \
            --platform linux/amd64 \
            --file Dockerfile.release \
            --tag sunscreen-llvm-builder:amd64 \
            --load \
            .

        # Extract tarball from container
        CONTAINER_ID=$(docker create sunscreen-llvm-builder:amd64)
        docker cp "${CONTAINER_ID}:/output/." "${OUTPUT_DIR}/"
        docker rm "${CONTAINER_ID}"

        # Validate and rename to expected filename
        EXTRACTED_TARBALL=$(find "${OUTPUT_DIR}" -name "parasol-compiler-linux-x86-64-*.tar.gz" -print -quit)
        if [[ -z "${EXTRACTED_TARBALL}" ]]; then
            echo "Error: No Linux x86-64 tarball found in Docker output"
            echo "Contents of ${OUTPUT_DIR}:"
            ls -la "${OUTPUT_DIR}"
            exit 1
        fi
        mv "${EXTRACTED_TARBALL}" "${OUTPUT_DIR}/${LINUX_X64_TARBALL}"
        echo "Linux x86-64 build complete: ${LINUX_X64_TARBALL}"
    else
        echo "[dry-run] Would build Linux x86-64 in Docker (amd64)"
    fi
else
    echo ""
    echo "Skipping Linux x86-64 build (--skip-linux-x64)"
fi

# Cleanup
if [[ "$DRY_RUN" == "false" && ("$BUILD_LINUX_ARM" == "true" || "$BUILD_LINUX_X64" == "true") ]]; then
    rm -f "${SCRIPT_DIR}/${SOURCE_TARBALL}"
fi

# Calculate hashes
echo ""
if [[ "$DRY_RUN" == "false" ]]; then
    echo "Calculating SHA256 hashes..."

    cd "${OUTPUT_DIR}"

    MACOS_HASH=$(nix-hash --type sha256 --base32 --flat "${MACOS_TARBALL}")
    LINUX_ARM_HASH=$(nix-hash --type sha256 --base32 --flat "${LINUX_ARM_TARBALL}")
    LINUX_X64_HASH=$(nix-hash --type sha256 --base32 --flat "${LINUX_X64_TARBALL}")

    cd "${SCRIPT_DIR}"

    echo "Hashes calculated:"
    echo "  macOS aarch64: ${MACOS_HASH}"
    echo "  Linux aarch64: ${LINUX_ARM_HASH}"
    echo "  Linux x86-64:  ${LINUX_X64_HASH}"
else
    echo "[dry-run] Would calculate SHA256 hashes"
    MACOS_HASH="<dry-run>"
    LINUX_ARM_HASH="<dry-run>"
    LINUX_X64_HASH="<dry-run>"
fi

# Update sunscreen-llvm.nix
echo ""
if [[ "$DRY_RUN" == "false" ]]; then
    echo "Updating sunscreen-llvm.nix..."

    NIX_FILE="${SCRIPT_DIR}/sunscreen-llvm.nix"

    # Update version
    sed -i.bak "s/version = \"[0-9]*\.[0-9]*\.[0-9]*\"/version = \"${VERSION}\"/" "${NIX_FILE}"

    # Update macOS hash
    sed -i.bak "/aarch64-darwin/,/sha256 =/ s/sha256 = \"[^\"]*\"/sha256 = \"${MACOS_HASH}\"/" "${NIX_FILE}"

    # Update Linux aarch64 hash
    sed -i.bak "/aarch64-linux/,/sha256 =/ s/sha256 = \"[^\"]*\"/sha256 = \"${LINUX_ARM_HASH}\"/" "${NIX_FILE}"

    # Update Linux x86-64 hash
    sed -i.bak "/x86_64-linux/,/sha256 =/ s/sha256 = \"[^\"]*\"/sha256 = \"${LINUX_X64_HASH}\"/" "${NIX_FILE}"

    # Remove backup file
    rm -f "${NIX_FILE}.bak"

    echo "sunscreen-llvm.nix updated successfully"
else
    echo "[dry-run] Would update sunscreen-llvm.nix with:"
    echo "  version: ${VERSION}"
    echo "  macOS hash: ${MACOS_HASH}"
    echo "  Linux ARM hash: ${LINUX_ARM_HASH}"
    echo "  Linux x64 hash: ${LINUX_X64_HASH}"
fi

# Summary
echo ""
echo "=========================================="
echo "Release build complete!"
echo "=========================================="
echo ""
echo "Version: ${VERSION}"
echo ""
echo "Tarballs created in ${OUTPUT_DIR}:"
echo "  - ${MACOS_TARBALL}"
echo "  - ${LINUX_ARM_TARBALL}"
echo "  - ${LINUX_X64_TARBALL}"
echo ""
echo "sunscreen-llvm.nix has been updated with new version and hashes."
echo ""
echo "Next steps:"
echo "  1. Review changes: git diff sunscreen-llvm.nix"
echo "  2. Commit changes: git add sunscreen-llvm.nix && git commit -m 'chore: release v${VERSION}'"
echo "  3. Create tag: git tag v${VERSION}"
echo "  4. Push tag: git push origin v${VERSION}"
echo "  5. Create GitHub release and upload tarballs from ${OUTPUT_DIR}"
echo ""
