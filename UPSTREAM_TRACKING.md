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

As of the 14.8.0.1 update, the fork carries **almost no patches** — both of its
historical reasons to exist were absorbed upstream. The `postPatch` /
`configureFlags` in `.flox/pkgs/sox_ng.nix` now consist of:

### Phase 1: Large Sinc Filter Support — UPSTREAM (since sox_ng 14.7.x)
- The large-tap sinc limit (`11 … 1073741823`) and a large `FFT4G_MAX_SIZE`
  (`1U << 31`) are now native in upstream. **No patching needed.**
- `postPatch` instead **asserts** these are present (`grep` guards) so a future
  upstream regression fails the build loudly instead of silently shipping
  32767-tap filters.
- The old `src/fft4g.c` `ip[256]→ip[16384]` patch is obsolete — upstream
  rewrote `fft4g.c` and its work arrays.

### Phase 2: sox_ng → sox Rebranding — UPSTREAM via `--enable-replace`
- Upstream ships a first-class `--enable-replace` configure flag that installs
  `sox`/`play`/`rec`/`soxi`, `libsox.{a,la,so}`, `sox.h`, `sox.pc` and all man
  page compatibility symlinks alongside the `sox_ng` originals.
- We simply pass `--enable-replace` in `configureFlags`. The old brute-force
  `mv`/`sed`/`substituteInPlace` renaming has been **removed** (it broke on
  every upstream build restructure).
- `sox_ng` remains the real binary; `sox` is a symlink to it (functionally a
  drop-in replacement; `mainProgram = "sox"`).

### Phase 3: Platform-Specific Fixes (Darwin, defensive)
- `uint64_t → sox_uint64_t` offset/function-pointer `sed` passes, Darwin-only.
- The old `lsx_rawseek` fix was removed — upstream already uses `sox_uint64_t`.

If a future upstream release removes the large-sinc support, the Phase-1
assertions will fail the build — that is the intended signal to review.

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

- **Upstream remote**: ✅ Added (`https://codeberg.org/sox_ng/sox_ng.git`)
- **Latest upstream tag**: `sox_ng-14.8.0.1`
- **Our version**: ✅ `14.8.0.1-custom` (updated 2026-07-18)
- **Source files**: ✅ Unpatched (matches the 14.8.0.1 tag exactly, minus fork-only files)
- **Patches**: ✅ Minimal — large-sinc + rebranding are now upstream (see Patch Maintenance)
- **Tested**: ✅ `flox build sox_ng` and `nix build .` both build; large sinc filters (50k/200k/500k taps) verified; over-max errors at 1.07e9

## Completed Update (2026-07-18): 14.6.1.2 → 14.8.0.1

A large jump (two minor versions, ~726 upstream commits). Key outcomes:
1. ✅ Selective source sync to the `sox_ng-14.8.0.1` tag (fork files preserved)
2. ✅ Retired the Phase-1 large-sinc patches — the feature is now native upstream
3. ✅ Replaced brute-force rebranding with upstream's `--enable-replace` flag
4. ✅ Removed the obsolete `sox_ng-linux` / `sox_ng-darwin` manifest build recipes
5. ✅ Both `flox build sox_ng` and `nix build .` working; functionality verified

### Earlier: 14.6.0.2 → 14.6.1.2 (2025-11-06)

The selective-checkout workflow was first validated updating 14.6.0.2 → 14.6.1.2
(handling the upstream `GETOPT_LOCAL_NUMERIC` change), with both build paths working.

## Post-Update Verification Checklist

After updating to a new version, verify everything works correctly:

### 1. Source Files Check (Should Remain UNPATCHED)
```bash
# FFT4G_MAX_SIZE should be 262144 (not 1073741824)
grep "FFT4G_MAX_SIZE" src/fft4g.h
# Should show: #define FFT4G_MAX_SIZE 262144

# ip array should be 256 (not 16384)
grep "int j, j1, k, k1, l, m, m2, ip\[" src/fft4g.c | head -1
# Should show: ip[256]

# sinc taps should be 32767 (not 1073741823)
grep "GETOPT.*'n'.*taps.*32767" src/sinc.c
# Should show: ...32767

# Version should match new upstream version
grep "AC_INIT" configure.ac
# Should show: AC_INIT([sox_ng], [14.6.X], ...)
```

### 2. Nix Expression Check (Should Have PATCHES)
```bash
# Version should match with -custom suffix
grep "version =" .flox/pkgs/sox_ng.nix | head -1
# Should show: version = "14.6.X-custom"

# Patches should be present
grep "1073741824" .flox/pkgs/sox_ng.nix  # FFT4G patch
grep "ip\[16384\]" .flox/pkgs/sox_ng.nix  # ip array patch
grep "1073741823" .flox/pkgs/sox_ng.nix  # sinc taps patch
# All three should return results
```

### 3. Build Check
```bash
# Both build methods should work
flox build sox_ng
nix --extra-experimental-features 'nix-command flakes' build .

# Check outputs exist
ls result-sox_ng/bin/sox
ls result/bin/sox
```

### 4. Binary Verification
```bash
# Version should show new upstream version
./result-sox_ng/bin/sox --version
# Should show: SoX_ng v14.6.X

# Binary should be named 'sox' not 'sox_ng'
ls result-sox_ng/bin/
# Should show: play, rec, sox, soxi (NOT sox_ng)

# Symlinks should work
./result-sox_ng/bin/play --version
./result-sox_ng/bin/rec --version
```

### 5. Functionality Verification
```bash
# Test audio processing works
./result-sox_ng/bin/sox -n test.flac synth 1 sine 440
ls -lh test.flac
# Should show file created

# Test large sinc filter (50k taps)
./result-sox_ng/bin/sox test.flac out1.flac sinc -n 50000 -20000
ls -lh out1.flac
# Should complete without errors

# Test very large sinc filter (500k taps)
./result-sox_ng/bin/sox test.flac out2.flac sinc -n 500000 -20000
ls -lh out2.flac
# Should complete without errors (may take time)

# Test limit boundary (should fail with correct error)
./result-sox_ng/bin/sox test.flac out3.flac sinc -n 1073741824 -20000 2>&1 | grep "must be"
# Should show: parameter `taps' must be from 11 to 1.07374e+09

# Cleanup
rm test.flac out1.flac out2.flac
```

### 6. Git Status Check
```bash
# Review what changed
git status
git diff --cached --stat

# Commit should include source updates and Nix expression version bump
git log -1 --stat
```

## Quick Reference

For a condensed, copy-paste-friendly update guide, see [QUICK_UPDATE_GUIDE.md](QUICK_UPDATE_GUIDE.md).

## Next Steps for Future Updates

1. Monitor upstream for new releases (check every 3-6 months)
2. When new version available, use the selective checkout workflow above
3. Test builds and verify patches still apply correctly
4. Update flake.lock if needed
5. Commit and push
