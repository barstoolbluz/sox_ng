#!/bin/bash
# batch_build_versions.sh - Test multiple sox_ng versions
#
# This script builds and tests multiple versions of sox_ng to verify
# that patches work across versions and large sinc filters function correctly.

set -e

# Configuration
VERSIONS=(
  "sox_ng-14.6.1.2"
  "sox_ng-14.6.1.1"
  "sox_ng-14.6.1"
  "sox_ng-14.6.0.4"
  "sox_ng-14.6.0.3"
)

TEST_TAPS=(50000 100000 500000)  # Test these tap counts
BUILD_DIR="/tmp/sox_ng_batch_builds"
LOG_FILE="$BUILD_DIR/build_results.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Setup
mkdir -p "$BUILD_DIR"
echo "Sox_ng Batch Build Test - $(date)" > "$LOG_FILE"
echo "=================================" >> "$LOG_FILE"
echo ""

# Get repository root
REPO_ROOT=$(git rev-parse --show-toplevel)

# For each version
for VERSION in "${VERSIONS[@]}"; do
  echo ""
  echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
  echo -e "${BLUE}║${NC} Testing: ${YELLOW}$VERSION${NC}"
  echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"

  VERSION_DIR="$BUILD_DIR/$VERSION"
  mkdir -p "$VERSION_DIR"

  # Check if version exists
  if ! git rev-parse "$VERSION" >/dev/null 2>&1; then
    echo -e "${RED}✗ $VERSION: TAG NOT FOUND${NC}" | tee -a "$LOG_FILE"
    continue
  fi

  # Export clean source
  echo "Exporting source..."
  git archive --format=tar "$VERSION" | tar -x -C "$VERSION_DIR"

  # Copy Nix expression
  mkdir -p "$VERSION_DIR/.flox/pkgs"
  cp "$REPO_ROOT/.flox/pkgs/sox_ng.nix" "$VERSION_DIR/.flox/pkgs/"

  # Update version
  VERSION_NUM=$(echo "$VERSION" | sed 's/sox_ng-//')
  sed -i "s/version = \".*-custom\"/version = \"${VERSION_NUM}-custom\"/" \
    "$VERSION_DIR/.flox/pkgs/sox_ng.nix"

  # Build
  cd "$VERSION_DIR"
  echo "Building..."

  if flox build sox_ng > "$VERSION_DIR/build.log" 2>&1; then
    echo -e "${GREEN}✓ $VERSION: BUILD SUCCESS${NC}" | tee -a "$LOG_FILE"

    # Test version
    VERSION_OUTPUT=$(./result-sox_ng/bin/sox --version 2>&1 || echo "unknown")
    echo "  Version: $VERSION_OUTPUT" | tee -a "$LOG_FILE"

    # Test binary naming
    if [ -f "./result-sox_ng/bin/sox" ] && [ ! -f "./result-sox_ng/bin/sox_ng" ]; then
      echo -e "  ${GREEN}✓${NC} Binary correctly named 'sox'" | tee -a "$LOG_FILE"
    else
      echo -e "  ${RED}✗${NC} Binary naming issue" | tee -a "$LOG_FILE"
    fi

    # Create test audio
    if ./result-sox_ng/bin/sox -n test.flac synth 1 sine 440 2>/dev/null; then
      echo -e "  ${GREEN}✓${NC} Audio generation works" | tee -a "$LOG_FILE"
    else
      echo -e "  ${RED}✗${NC} Audio generation failed" | tee -a "$LOG_FILE"
      cd "$REPO_ROOT"
      continue
    fi

    # Test large sinc filters
    for TAPS in "${TEST_TAPS[@]}"; do
      if timeout 120 ./result-sox_ng/bin/sox test.flac out.flac sinc -n "$TAPS" -20000 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} ${TAPS} taps: SUCCESS" | tee -a "$LOG_FILE"
      else
        echo -e "  ${RED}✗${NC} ${TAPS} taps: FAILED" | tee -a "$LOG_FILE"
      fi
      rm -f out.flac
    done

    # Test boundary
    if ./result-sox_ng/bin/sox test.flac out.flac sinc -n 1073741824 -20000 2>&1 | \
       grep -q "1.07374e+09"; then
      echo -e "  ${GREEN}✓${NC} Limit boundary correct (1.07e9)" | tee -a "$LOG_FILE"
    else
      echo -e "  ${YELLOW}?${NC} Limit boundary check unclear" | tee -a "$LOG_FILE"
    fi

    rm -f test.flac out.flac

  else
    echo -e "${RED}✗ $VERSION: BUILD FAILED${NC}" | tee -a "$LOG_FILE"
    echo "  See: $VERSION_DIR/build.log"
    tail -20 "$VERSION_DIR/build.log" | tee -a "$LOG_FILE"
  fi

  cd "$REPO_ROOT"
done

echo ""
echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║${NC} Build Summary"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
cat "$LOG_FILE"
echo ""
echo "Full results in: $BUILD_DIR"
echo "Log file: $LOG_FILE"
echo ""
echo "To cleanup: rm -rf $BUILD_DIR"
