# Building Intermediate Versions

This guide shows how to build and test intermediate versions of sox_ng without permanently updating your repository. Useful for testing multiple versions or troubleshooting regressions.

## Current Status

```bash
# Check what you currently have
grep "AC_INIT" configure.ac
# Shows: AC_INIT([sox_ng], [14.6.1.2], ...)

# Latest stable upstream
git tag -l 'sox_ng-14.6.*' --sort=-version:refname | grep -v "rc\|git" | head -1
# Shows: sox_ng-14.6.1.2
```

You're on the **latest stable version** (14.6.1.2).

## List Available Versions

```bash
# All 14.6.x releases (stable only)
git tag -l 'sox_ng-14.6.*' --sort=-version:refname | grep -v "rc\|git"

# All versions including release candidates and git snapshots
git tag -l 'sox_ng-14.6.*' --sort=-version:refname

# All stable versions across all branches
git tag -l 'sox_ng-*' --sort=-version:refname | grep -v "rc\|git" | head -20
```

## Method 1: Temporary Checkout (Non-Destructive)

Build a version **without modifying your git working tree**:

```bash
#!/bin/bash
# Build sox_ng version without changing working tree

VERSION="sox_ng-14.6.0"  # Replace with desired version

# Create temporary build directory
BUILD_DIR="/tmp/sox_ng_build_${VERSION}"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Export version to temporary directory
git archive --format=tar "$VERSION" | tar -x -C "$BUILD_DIR"

# Copy our Nix expression patches
cp .flox/pkgs/sox_ng.nix "$BUILD_DIR/.flox/pkgs/"

# Update version string in Nix expression
cd "$BUILD_DIR"
VERSION_NUM=$(echo "$VERSION" | sed 's/sox_ng-//')
sed -i "s/version = \".*-custom\"/version = \"${VERSION_NUM}-custom\"/" .flox/pkgs/sox_ng.nix

# Build
flox build sox_ng

# Test
./result-sox_ng/bin/sox --version
./result-sox_ng/bin/sox -n test.flac synth 1 sine 440
./result-sox_ng/bin/sox test.flac out.flac sinc -n 100000 -20000

echo "Build complete in: $BUILD_DIR"
echo "To inspect: cd $BUILD_DIR"
```

**Advantage**: Your working tree stays clean. Great for testing.

## Method 2: Branch-Based Testing (Organized)

Build multiple versions systematically using git branches:

```bash
#!/bin/bash
# Test multiple versions on separate branches

VERSIONS=(
  "sox_ng-14.6.1.2"
  "sox_ng-14.6.1.1"
  "sox_ng-14.6.1"
  "sox_ng-14.6.0.4"
)

for VERSION in "${VERSIONS[@]}"; do
  echo "================================"
  echo "Building $VERSION"
  echo "================================"

  # Create test branch
  BRANCH="test/$VERSION"
  git checkout -b "$BRANCH" 2>/dev/null || git checkout "$BRANCH"

  # Checkout source for this version
  git checkout "$VERSION" -- src/ configure.ac Makefile.am CMakeLists.txt

  # Update Nix expression version
  VERSION_NUM=$(echo "$VERSION" | sed 's/sox_ng-//')
  sed -i "s/version = \".*-custom\"/version = \"${VERSION_NUM}-custom\"/" .flox/pkgs/sox_ng.nix

  # Build
  flox build sox_ng

  if [ $? -eq 0 ]; then
    echo "✓ $VERSION built successfully"
    ./result-sox_ng/bin/sox --version

    # Optional: Test large sinc filters
    ./result-sox_ng/bin/sox -n test.flac synth 1 sine 440
    ./result-sox_ng/bin/sox test.flac out.flac sinc -n 100000 -20000
    rm -f test.flac out.flac

  else
    echo "✗ $VERSION build failed"
  fi

  # Return to main
  git checkout main
done

# Cleanup test branches (optional)
# git branch | grep "test/sox_ng" | xargs git branch -D
```

**Advantage**: Keeps each version in a separate branch for easy comparison.

## Method 3: Worktree-Based Testing (Advanced)

Use git worktrees to build multiple versions in parallel directories:

```bash
#!/bin/bash
# Build multiple versions in parallel using worktrees

BASE_DIR="/tmp/sox_ng_worktrees"
mkdir -p "$BASE_DIR"

VERSIONS=(
  "sox_ng-14.6.1.2"
  "sox_ng-14.6.1.1"
  "sox_ng-14.6.1"
)

for VERSION in "${VERSIONS[@]}"; do
  WORKTREE_DIR="$BASE_DIR/$VERSION"

  # Create worktree
  if [ ! -d "$WORKTREE_DIR" ]; then
    git worktree add "$WORKTREE_DIR" main
  fi

  cd "$WORKTREE_DIR"

  # Checkout version
  git checkout "$VERSION" -- src/ configure.ac Makefile.am CMakeLists.txt

  # Update Nix expression
  VERSION_NUM=$(echo "$VERSION" | sed 's/sox_ng-//')
  sed -i "s/version = \".*-custom\"/version = \"${VERSION_NUM}-custom\"/" .flox/pkgs/sox_ng.nix

  # Build in background
  (
    flox build sox_ng
    echo "✓ $VERSION built in $WORKTREE_DIR"
  ) &
done

# Wait for all builds
wait

echo "All builds complete. Check: $BASE_DIR"

# Cleanup worktrees when done:
# git worktree list
# git worktree remove /tmp/sox_ng_worktrees/sox_ng-14.6.1.2
```

**Advantage**: Build multiple versions in parallel, each in its own directory.

## Method 4: In-Place Version Switching (Simplest)

Switch versions directly in your working tree (like normal updates):

```bash
#!/bin/bash
# Switch between versions in-place

# Save current state
git stash

# Checkout desired version
VERSION="sox_ng-14.6.1.1"
git checkout "$VERSION" -- src/ configure.ac Makefile.am CMakeLists.txt

# Update Nix expression
VERSION_NUM=$(echo "$VERSION" | sed 's/sox_ng-//')
sed -i "s/version = \".*-custom\"/version = \"${VERSION_NUM}-custom\"/" .flox/pkgs/sox_ng.nix

# Build
flox build sox_ng
./result-sox_ng/bin/sox --version

# To return to latest:
# git checkout main -- src/ configure.ac Makefile.am CMakeLists.txt .flox/pkgs/sox_ng.nix
# git stash pop
```

**Advantage**: Simple, no extra directories. **Disadvantage**: Changes your working tree.

## Batch Build Script

Here's a complete script to test multiple versions systematically:

```bash
#!/bin/bash
# batch_build_versions.sh - Test multiple sox_ng versions

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

# Setup
mkdir -p "$BUILD_DIR"
echo "Sox_ng Batch Build Test - $(date)" > "$LOG_FILE"
echo "=================================" >> "$LOG_FILE"

# For each version
for VERSION in "${VERSIONS[@]}"; do
  echo ""
  echo "╔════════════════════════════════════════════════════════════════╗"
  echo "║ Testing: $VERSION"
  echo "╚════════════════════════════════════════════════════════════════╝"

  VERSION_DIR="$BUILD_DIR/$VERSION"
  mkdir -p "$VERSION_DIR"

  # Export clean source
  git archive --format=tar "$VERSION" | tar -x -C "$VERSION_DIR"

  # Copy Nix expression
  mkdir -p "$VERSION_DIR/.flox/pkgs"
  cp .flox/pkgs/sox_ng.nix "$VERSION_DIR/.flox/pkgs/"

  # Update version
  VERSION_NUM=$(echo "$VERSION" | sed 's/sox_ng-//')
  sed -i "s/version = \".*-custom\"/version = \"${VERSION_NUM}-custom\"/" \
    "$VERSION_DIR/.flox/pkgs/sox_ng.nix"

  # Build
  cd "$VERSION_DIR"
  echo "Building..." | tee -a "$LOG_FILE"

  if flox build sox_ng 2>&1 | tee "$VERSION_DIR/build.log"; then
    echo "✓ $VERSION: BUILD SUCCESS" | tee -a "$LOG_FILE"

    # Test version
    VERSION_OUTPUT=$(./result-sox_ng/bin/sox --version)
    echo "  Version: $VERSION_OUTPUT" | tee -a "$LOG_FILE"

    # Test binary naming
    if [ -f "./result-sox_ng/bin/sox" ] && [ ! -f "./result-sox_ng/bin/sox_ng" ]; then
      echo "  ✓ Binary correctly named 'sox'" | tee -a "$LOG_FILE"
    else
      echo "  ✗ Binary naming issue" | tee -a "$LOG_FILE"
    fi

    # Test large sinc filters
    ./result-sox_ng/bin/sox -n test.flac synth 1 sine 440 2>/dev/null

    for TAPS in "${TEST_TAPS[@]}"; do
      if ./result-sox_ng/bin/sox test.flac out.flac sinc -n "$TAPS" -20000 2>/dev/null; then
        echo "  ✓ ${TAPS} taps: SUCCESS" | tee -a "$LOG_FILE"
      else
        echo "  ✗ ${TAPS} taps: FAILED" | tee -a "$LOG_FILE"
      fi
    done

    # Test boundary
    if ./result-sox_ng/bin/sox test.flac out.flac sinc -n 1073741824 -20000 2>&1 | \
       grep -q "1.07374e+09"; then
      echo "  ✓ Limit boundary correct (1.07e9)" | tee -a "$LOG_FILE"
    else
      echo "  ✗ Limit boundary incorrect" | tee -a "$LOG_FILE"
    fi

    rm -f test.flac out.flac

  else
    echo "✗ $VERSION: BUILD FAILED" | tee -a "$LOG_FILE"
    echo "  See: $VERSION_DIR/build.log"
  fi

  cd - > /dev/null
done

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║ Build Summary"
echo "╚════════════════════════════════════════════════════════════════╝"
cat "$LOG_FILE"
echo ""
echo "Full results in: $BUILD_DIR"
echo "Log file: $LOG_FILE"
```

Save this as `batch_build_versions.sh`, make it executable, and run:

```bash
chmod +x batch_build_versions.sh
./batch_build_versions.sh
```

## Testing for Regressions

To find when a regression was introduced:

```bash
#!/bin/bash
# binary_search_regression.sh - Find version that introduced a bug

# Define good and bad versions
GOOD_VERSION="sox_ng-14.6.0"
BAD_VERSION="sox_ng-14.6.1.2"

# Get all versions between them
VERSIONS=$(git tag -l 'sox_ng-14.6.*' --sort=version:refname | \
  sed -n "/$GOOD_VERSION/,/$BAD_VERSION/p")

echo "Testing versions between $GOOD_VERSION and $BAD_VERSION"

# Binary search through versions
for VERSION in $VERSIONS; do
  echo "Testing $VERSION..."

  # Build version (using Method 1)
  BUILD_DIR="/tmp/test_$VERSION"
  rm -rf "$BUILD_DIR"
  mkdir -p "$BUILD_DIR"
  git archive --format=tar "$VERSION" | tar -x -C "$BUILD_DIR"
  cp .flox/pkgs/sox_ng.nix "$BUILD_DIR/.flox/pkgs/"

  cd "$BUILD_DIR"
  VERSION_NUM=$(echo "$VERSION" | sed 's/sox_ng-//')
  sed -i "s/version = \".*-custom\"/version = \"${VERSION_NUM}-custom\"/" .flox/pkgs/sox_ng.nix

  flox build sox_ng 2>&1 | grep -q "success"

  # Run your regression test here
  # Replace this with your actual test
  if ./result-sox_ng/bin/sox -n test.flac synth 1 sine 440 && \
     ./result-sox_ng/bin/sox test.flac out.flac sinc -n 100000 -20000; then
    echo "✓ $VERSION: PASSES"
  else
    echo "✗ $VERSION: REGRESSION FOUND"
    echo "First bad version: $VERSION"
    break
  fi

  cd - > /dev/null
done
```

## Comparing Built Binaries

After building multiple versions:

```bash
# Compare binary sizes
for version in /tmp/sox_ng_worktrees/*/result-sox_ng/bin/sox; do
  ls -lh "$version"
done

# Compare versions
for version in /tmp/sox_ng_worktrees/*/result-sox_ng/bin/sox; do
  echo "$version:"
  "$version" --version
done

# Benchmark performance
for version in /tmp/sox_ng_worktrees/*/result-sox_ng/bin/sox; do
  echo "Testing: $version"
  time "$version" -n test.flac synth 10 sine 440
  time "$version" test.flac out.flac sinc -n 500000 -20000
  rm test.flac out.flac
done
```

## Cleanup

```bash
# Remove temporary build directories
rm -rf /tmp/sox_ng_build_*
rm -rf /tmp/sox_ng_batch_builds

# Remove test branches
git branch | grep "test/sox_ng" | xargs git branch -D

# Remove worktrees
git worktree list
git worktree prune

# Return to clean state
git checkout main
git checkout -- .
```

## Recommendation

For most use cases, I recommend **Method 1 (Temporary Checkout)**:
- Non-destructive (doesn't modify your working tree)
- Simple and safe
- Easy to script
- Can test multiple versions without git branch complexity

For systematic testing of many versions, use the **Batch Build Script** provided above.

## See Also

- [QUICK_UPDATE_GUIDE.md](QUICK_UPDATE_GUIDE.md) - For updating to latest version
- [UPSTREAM_TRACKING.md](UPSTREAM_TRACKING.md) - For detailed tracking workflow
