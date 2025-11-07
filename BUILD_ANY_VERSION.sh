#!/bin/bash
# BUILD_ANY_VERSION.sh - Build any sox_ng version without modifying your repo
#
# Usage: ./BUILD_ANY_VERSION.sh sox_ng-14.6.1.1
#        ./BUILD_ANY_VERSION.sh sox_ng-14.6.0
#
# This builds the version in a temporary directory and leaves your repo untouched.

set -e

# Check if version specified
if [ $# -eq 0 ]; then
    echo "Usage: $0 <version-tag>"
    echo ""
    echo "Available 14.6.x versions:"
    git tag -l 'sox_ng-14.6.*' --sort=-version:refname | grep -v "rc\|git" | head -10
    echo ""
    echo "Example: $0 sox_ng-14.6.1.1"
    exit 1
fi

VERSION="$1"

# Verify version exists
if ! git rev-parse "$VERSION" >/dev/null 2>&1; then
    echo "Error: Version '$VERSION' not found"
    echo ""
    echo "Available versions:"
    git tag -l 'sox_ng-*' --sort=-version:refname | grep -v "rc\|git" | head -20
    exit 1
fi

echo "════════════════════════════════════════════════════════════════"
echo "Building sox_ng version: $VERSION"
echo "════════════════════════════════════════════════════════════════"
echo ""

# Create build directory
BUILD_DIR="/tmp/sox_ng_build_${VERSION}"
echo "Build directory: $BUILD_DIR"

# Clean up old build if exists
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning up previous build..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

# Get repository root
REPO_ROOT=$(git rev-parse --show-toplevel)

# Export source
echo "Exporting source from git..."
git archive --format=tar "$VERSION" | tar -x -C "$BUILD_DIR"

# Copy Nix expression
echo "Copying Nix expression..."
mkdir -p "$BUILD_DIR/.flox/pkgs"
cp "$REPO_ROOT/.flox/pkgs/sox_ng.nix" "$BUILD_DIR/.flox/pkgs/"

# Update version string
VERSION_NUM=$(echo "$VERSION" | sed 's/sox_ng-//')
echo "Setting version to: ${VERSION_NUM}-custom"
sed -i "s/version = \".*-custom\"/version = \"${VERSION_NUM}-custom\"/" \
    "$BUILD_DIR/.flox/pkgs/sox_ng.nix"

# Build
echo ""
echo "════════════════════════════════════════════════════════════════"
echo "Building... (this may take a few minutes)"
echo "════════════════════════════════════════════════════════════════"
cd "$BUILD_DIR"

if flox build sox_ng; then
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "✓ BUILD SUCCESSFUL"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    echo "Version:"
    ./result-sox_ng/bin/sox --version
    echo ""
    echo "Binary location:"
    ls -lh ./result-sox_ng/bin/sox
    echo ""
    echo "Build directory: $BUILD_DIR"
    echo ""
    echo "Quick test:"
    echo "  cd $BUILD_DIR"
    echo "  ./result-sox_ng/bin/sox -n test.flac synth 1 sine 440"
    echo "  ./result-sox_ng/bin/sox test.flac out.flac sinc -n 100000 -20000"
    echo ""
    echo "To cleanup:"
    echo "  rm -rf $BUILD_DIR"
    echo ""
else
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "✗ BUILD FAILED"
    echo "════════════════════════════════════════════════════════════════"
    echo "Check build log in: $BUILD_DIR"
    exit 1
fi
