# Upstream Tracking Workflow

This document describes how to track and incorporate updates from the upstream sox_ng repository.

## Repository Structure

- **Origin**: `https://github.com/barstoolbluz/sox_ng` (this fork)
- **Upstream**: `https://codeberg.org/sox_ng/sox_ng` (original sox_ng)
- **Branch**: `main` (both repositories)

## Key Principle

**The source tree remains UNPATCHED**. All modifications (large sinc filters, renaming, Darwin fixes) are applied during build time via the Nix expression at `.flox/pkgs/sox_ng.nix`.

This approach provides:
- Clean separation between upstream source and our patches
- Easy updates from upstream (just merge/rebase)
- Patches live in one place (the Nix expression)
- No conflicts between patch commits and upstream commits

## Initial Setup (Already Complete)

```bash
# Add upstream remote
git remote add upstream https://codeberg.org/sox_ng/sox_ng.git

# Fetch upstream branches and tags
git fetch upstream
```

## Updating from Upstream

### 1. Check for New Releases

```bash
# Fetch latest upstream changes
git fetch upstream

# List new tags
git tag -l 'sox_ng-*' | tail -10

# Check latest upstream commit
git log upstream/main -5
```

### 2. Update Source Files from Upstream

Since this repository was created by importing sox_ng source (not forking), we use a **selective update** approach:

**Important**: We only update the sox_ng source files, NOT our custom files (.flox/, flake.nix, *.md docs, etc.)

```bash
# Create a temporary directory for the new source
mkdir -p /tmp/sox_ng_update
cd /tmp/sox_ng_update

# Get the desired upstream tag (example: sox_ng-14.6.1.2)
UPSTREAM_TAG="sox_ng-14.6.1.2"

# Clone just that tag
git clone --depth 1 --branch $UPSTREAM_TAG https://codeberg.org/sox_ng/sox_ng.git source

# Go back to your repository
cd /home/daedalus/dev/builds/sox_ng

# Create a list of files to preserve (our custom files)
cat > /tmp/preserve_files.txt << 'EOF'
.flox/
flake.nix
flake.lock
.git/
.gitignore
README.md
UPSTREAM_TRACKING.md
EXTREME_RESAMPLING_WITH_SOX_A_GUIDE_FOR_THE_PERPLEXED.md
LARGE_SINC_FILTERS_IMPLEMENTATION.md
SOX_DSD_BASICS.md
FIR_CHANGES_ANALYSIS.md
FLOX.md
EOF

# Backup our custom files
mkdir -p /tmp/sox_ng_backup
tar czf /tmp/sox_ng_backup/custom_files.tar.gz \
  .flox flake.nix flake.lock README.md UPSTREAM_TRACKING.md \
  EXTREME_RESAMPLING_WITH_SOX_A_GUIDE_FOR_THE_PERPLEXED.md \
  LARGE_SINC_FILTERS_IMPLEMENTATION.md SOX_DSD_BASICS.md \
  FIR_CHANGES_ANALYSIS.md FLOX.md 2>/dev/null || true

# Remove old sox_ng source files (but keep our custom files)
# Use find to exclude our custom directories
find . -maxdepth 1 -type f ! -name 'README.md' \
  ! -name 'UPSTREAM_TRACKING.md' \
  ! -name 'EXTREME_RESAMPLING_WITH_SOX_A_GUIDE_FOR_THE_PERPLEXED.md' \
  ! -name 'LARGE_SINC_FILTERS_IMPLEMENTATION.md' \
  ! -name 'SOX_DSD_BASICS.md' \
  ! -name 'FIR_CHANGES_ANALYSIS.md' \
  ! -name 'FLOX.md' \
  ! -name 'flake.nix' \
  ! -name 'flake.lock' \
  -delete

find . -maxdepth 1 -type d ! -name '.' ! -name '.git' ! -name '.flox' -exec rm -rf {} + 2>/dev/null || true

# Copy new upstream source
rsync -av --exclude='.git' /tmp/sox_ng_update/source/ .

# Restore our custom files (in case any were overwritten)
tar xzf /tmp/sox_ng_backup/custom_files.tar.gz

# Add all changes
git add -A

# Check what changed
git status
git diff --cached --stat

# Commit the update
git commit -m "Update sox_ng source to $UPSTREAM_TAG"
```

**Alternative (Simpler) Method**: Manual selective checkout

```bash
# Checkout specific upstream files/directories
UPSTREAM_TAG="sox_ng-14.6.1.2"

# Source code
git checkout $UPSTREAM_TAG -- src/
git checkout $UPSTREAM_TAG -- configure.ac
git checkout $UPSTREAM_TAG -- Makefile.am
git checkout $UPSTREAM_TAG -- CMakeLists.txt

# Man pages (may need renaming patches)
git checkout $UPSTREAM_TAG -- sox_ng.1 soxi_ng.1 soxformat_ng.7 libsox_ng.3 2>/dev/null || true

# Other directories (avoid overwriting our docs)
git checkout $UPSTREAM_TAG -- m4/ scripts/ msvc10/ msvc9/ 2>/dev/null || true

# Commit
git commit -m "Update sox_ng source to $UPSTREAM_TAG"
```

**Recommendation**: Use the simpler **selective checkout** method. It's cleaner and less error-prone.

### 3. Update Version in Nix Expression

After merging/rebasing, update the version number in `.flox/pkgs/sox_ng.nix`:

```nix
stdenv.mkDerivation rec {
  pname = "sox_ng";
  version = "14.6.2-custom";  # Update this line
  # ...
}
```

### 4. Test the Build

```bash
# Test Nix expression build
flox build sox_ng

# Test flake build
nix build .

# Verify patches applied correctly
./result-sox_ng/bin/sox --help | grep -i "sinc"
./result/bin/sox --version

# Test large sinc filters still work
./result-sox_ng/bin/sox -n -r 96000 -c 2 -b 24 test.flac synth 1 sine 440
./result-sox_ng/bin/sox test.flac -r 192000 output.flac sinc -n 100000 -b 16
```

### 5. Update Flake Lock

```bash
# Update flake dependencies
nix flake update

# Or just update nixpkgs
nix flake lock --update-input nixpkgs
```

### 6. Commit and Push

```bash
git add .flox/pkgs/sox_ng.nix flake.lock
git commit -m "Update to sox_ng 14.6.2"
git push origin main
```

## Monitoring Upstream

### Check for Updates Regularly

```bash
# Fetch and show if upstream has new commits
git fetch upstream
git log HEAD..upstream/main --oneline

# Show new tags since last update
git tag -l 'sox_ng-*' --sort=-version:refname | head -5
```

### Subscribe to Upstream Releases

- Watch the upstream repository: https://codeberg.org/sox_ng/sox_ng
- Enable notifications for new releases
- sox_ng follows a six-monthly release cadence

## Patch Maintenance

All patches are maintained in `.flox/pkgs/sox_ng.nix` in the `postPatch` section:

### Phase 1: Large Sinc Filter Patches
- `src/fft4g.h`: FFT4G_MAX_SIZE 262144 → 1073741824
- `src/fft4g.c`: ip[256] → ip[16384]
- `src/sinc.c`: max taps 32767 → 1073741823

### Phase 2: sox_ng → sox Rebranding
- Comprehensive renaming across build files, source, docs, tests

### Phase 3: Platform-Specific Fixes
- Darwin: uint64_t → sox_uint64_t type fixes

If upstream changes affect these files, the Nix expression patches may need adjustment. The `substituteInPlace` commands use `--replace` with exact string matching, so upstream changes to those strings will cause build failures (which is good - it alerts us to review the patches).

## Troubleshooting

### Build Fails After Upstream Update

1. Check build output for which patch failed:
   ```bash
   flox build sox_ng 2>&1 | less
   ```

2. Common causes:
   - Upstream changed a line we're patching → Update the `--replace` pattern in Nix expression
   - Upstream renamed/moved a file → Update file paths in `postPatch`
   - Upstream already fixed something we patch → Remove that patch

3. Compare upstream changes to our patches:
   ```bash
   # See what changed in a specific file since last update
   git diff OLD_TAG..upstream/main -- src/sinc.c
   ```

### Merge Conflicts

If conflicts occur during merge/rebase:

```bash
# List conflicted files
git status

# For each conflict, choose strategy:
# 1. Keep upstream version (for source files we don't modify):
git checkout --theirs path/to/file

# 2. Keep our version (for our custom files like Nix expressions):
git checkout --ours path/to/file

# 3. Manually resolve (edit the file to fix conflicts)
# After resolving all conflicts:
git add .
git merge --continue  # or git rebase --continue
```

## Example: Full Update Workflow

```bash
# 1. Fetch upstream
git fetch upstream

# 2. Check what's new
git log HEAD..upstream/main --oneline
git tag -l 'sox_ng-*' --sort=-version:refname | head -3

# 3. Merge upstream
git merge upstream/main
# Or: git rebase upstream/main

# 4. Update version
$EDITOR .flox/pkgs/sox_ng.nix
# Change version = "14.6.1-custom" to version = "14.6.2-custom"

# 5. Test builds
flox build sox_ng
nix build .

# 6. Test functionality
./result-sox_ng/bin/sox --version
./result-sox_ng/bin/sox -n test.flac synth 1 sine 440
./result-sox_ng/bin/sox test.flac out.flac sinc -n 100000

# 7. Update locks and commit
nix flake update
git add .flox/pkgs/sox_ng.nix flake.lock
git commit -m "Update to sox_ng 14.6.2"
git push origin main
```

## Current Status

- **Upstream remote**: ✅ Added
- **Latest upstream tag**: `sox_ng-14.6.1.2`
- **Latest upstream commit**: `c9d20325` (sox_ng-14.6.1+git20251029-32)
- **Our version**: `14.6.0-custom` (needs update)
- **Source files**: Unpatched (patches applied at build time)
- **Patches**: All maintained in `.flox/pkgs/sox_ng.nix`

## Next Steps

1. Merge latest upstream (`sox_ng-14.6.1.2` or later)
2. Update version string in Nix expression
3. Test builds on both Linux and macOS if possible
4. Update flake.lock
5. Commit and push
