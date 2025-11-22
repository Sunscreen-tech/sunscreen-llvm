#!/usr/bin/env bash
set -euo pipefail

# Publish a GitHub release with pre-built tarballs
#
# Usage: ./publish-release.sh VERSION
#   VERSION: Release version in YYYY.MM.DD format (e.g., 2025.11.21)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RELEASES_DIR="${SCRIPT_DIR}/releases"

# Parse version argument
if [[ $# -ne 1 ]]; then
    echo "Error: VERSION argument required"
    echo "Usage: ./publish-release.sh VERSION"
    echo "Example: ./publish-release.sh 2025.11.21"
    exit 1
fi

VERSION="$1"

# Validate version format
if ! [[ "${VERSION}" =~ ^[0-9]{4}\.[0-9]{2}\.[0-9]{2}$ ]]; then
    echo "Error: VERSION must be in YYYY.MM.DD format, got: ${VERSION}"
    exit 1
fi

# Convert version for filename (periods to hyphens)
FILE_VERSION="${VERSION//./-}"

# Define expected tarball names
MACOS_TARBALL="parasol-compiler-macos-aarch64-${FILE_VERSION}.tar.gz"
LINUX_ARM_TARBALL="parasol-compiler-linux-aarch64-${FILE_VERSION}.tar.gz"
LINUX_X64_TARBALL="parasol-compiler-linux-x86-64-${FILE_VERSION}.tar.gz"

# Verify all tarballs exist
echo "Checking for release tarballs..."
MISSING=false
for tarball in "${MACOS_TARBALL}" "${LINUX_ARM_TARBALL}" "${LINUX_X64_TARBALL}"; do
    if [[ ! -f "${RELEASES_DIR}/${tarball}" ]]; then
        echo "  ✗ ${tarball} not found"
        MISSING=true
    else
        SIZE=$(du -h "${RELEASES_DIR}/${tarball}" | cut -f1)
        echo "  ✓ ${tarball} (${SIZE})"
    fi
done

if [[ "$MISSING" == "true" ]]; then
    echo ""
    echo "Error: Missing required tarballs in ${RELEASES_DIR}"
    echo "Run ./release-all.sh ${VERSION} to build them"
    exit 1
fi

# Check gh CLI is installed
if ! command -v gh &> /dev/null; then
    echo "Error: gh CLI not found. Install from https://cli.github.com/"
    exit 1
fi

# Check git status
if [[ -n $(git status --porcelain) ]]; then
    echo "Error: Git working directory is not clean"
    echo "Commit or stash changes before publishing release"
    exit 1
fi

# Check we're on sunscreen branch
CURRENT_BRANCH=$(git branch --show-current)
if [[ "${CURRENT_BRANCH}" != "sunscreen" ]]; then
    echo "Warning: Current branch is '${CURRENT_BRANCH}', not 'sunscreen'"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

TAG="v${VERSION}"

# Check if tag already exists
if git rev-parse "${TAG}" >/dev/null 2>&1; then
    echo "Error: Tag ${TAG} already exists"
    exit 1
fi

echo ""
echo "=========================================="
echo "Ready to publish release ${VERSION}"
echo "=========================================="
echo ""
echo "This will:"
echo "  1. Create git tag: ${TAG}"
echo "  2. Push tag to origin"
echo "  3. Create GitHub release with tarballs:"
echo "     - ${MACOS_TARBALL}"
echo "     - ${LINUX_ARM_TARBALL}"
echo "     - ${LINUX_X64_TARBALL}"
echo ""
read -p "Continue? (y/N) " -n 1 -r
echo

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted"
    exit 1
fi

# Create and push tag
echo ""
echo "Creating tag ${TAG}..."
git tag -a "${TAG}" -m "Release ${VERSION}"

echo "Pushing tag to origin..."
git push origin "${TAG}"

# Create GitHub release
echo ""
echo "Creating GitHub release..."
gh release create "${TAG}" \
    --title "Release ${VERSION}" \
    --notes "Parasol compiler release ${VERSION}

## Platform Binaries

- **macOS ARM64** (Apple Silicon): \`${MACOS_TARBALL}\`
- **Linux ARM64**: \`${LINUX_ARM_TARBALL}\`
- **Linux x86-64**: \`${LINUX_X64_TARBALL}\`

Each tarball contains:
- \`bin/clang\` - Parasol-enabled Clang compiler
- \`bin/ld.lld\` - LLD linker
- \`bin/llvm-objdump\` - Object file dumper
- \`bin/llvm-readelf\` - ELF file reader
- \`lib/clang/18/include/parasol.h\` - Parasol standard library

## Installation

\`\`\`bash
# Extract tarball
tar -xzf parasol-compiler-<platform>-${FILE_VERSION}.tar.gz

# Add to PATH
export PATH=\"\$PWD/bin:\$PATH\"

# Verify installation
clang --version
\`\`\`" \
    "${RELEASES_DIR}/${MACOS_TARBALL}" \
    "${RELEASES_DIR}/${LINUX_ARM_TARBALL}" \
    "${RELEASES_DIR}/${LINUX_X64_TARBALL}"

echo ""
echo "=========================================="
echo "Release published successfully!"
echo "=========================================="
echo ""
echo "View at: https://github.com/sunscreen-tech/sunscreen-llvm/releases/tag/${TAG}"
echo ""
