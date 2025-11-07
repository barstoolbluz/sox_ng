# Quick Update Guide

This is a concise reference for updating sox_ng from upstream. For detailed explanations, see [UPSTREAM_TRACKING.md](UPSTREAM_TRACKING.md).

## TL;DR - Update in 5 Steps

```bash
# 1. Fetch latest upstream
git fetch upstream --tags

# 2. Checkout new version (replace with actual tag)
git checkout sox_ng-14.6.2 -- src/ configure.ac Makefile.am CMakeLists.txt

# 3. Update version in Nix expression
sed -i 's/version = "14.6.1.2-custom"/version = "14.6.2-custom"/' .flox/pkgs/sox_ng.nix

# 4. Test build
flox build sox_ng
./result-sox_ng/bin/sox --version

# 5. Commit
git add -A
git commit -m "Update sox_ng source to sox_ng-14.6.2"
git push origin main
```

## Pre-Flight Checklist

Before updating, verify your environment:

```bash
# Check remotes are configured
git remote -v | grep upstream
# Should show: upstream https://codeberg.org/sox_ng/sox_ng.git

# Check current version
grep "version =" .flox/pkgs/sox_ng.nix | head -1
# Should show: version = "14.6.1.2-custom";

# Verify source is unpatched (should be 262144, not 1073741824)
grep "FFT4G_MAX_SIZE" src/fft4g.h
# Should show: #define FFT4G_MAX_SIZE 262144
```

## Finding New Releases

```bash
# List latest upstream tags
git fetch upstream --tags
git tag -l 'sox_ng-*' --sort=-version:refname | head -10

# See what's new in latest upstream
git log --oneline HEAD..upstream/main | head -20

# Check if there are new releases on specific branch
git log --oneline HEAD..upstream/14.6.X | head -10
```

## Post-Update Verification

After building, verify patches were applied correctly:

```bash
# Test version
./result-sox_ng/bin/sox --version
# Should show: SoX_ng v14.6.X (where X is the new version)

# Test binary naming
ls result-sox_ng/bin/
# Should show: sox (NOT sox_ng), plus symlinks: play, rec, soxi

# Test large sinc filters work
./result-sox_ng/bin/sox -n test.flac synth 1 sine 440
./result-sox_ng/bin/sox test.flac out.flac sinc -n 100000 -20000
# Should complete without errors
rm test.flac out.flac

# Test new limit boundary
./result-sox_ng/bin/sox -n /dev/null synth 0 sine 440 sinc -n 1073741824 -20000 2>&1 | grep "must be"
# Should show: parameter `taps' must be from 11 to 1.07374e+09
```

## If Something Goes Wrong

### Build Fails with Patch Error

If you see errors like "string to replace not found":

```bash
# Check what changed in the problematic file
PREVIOUS_TAG="sox_ng-14.6.1.2"
NEW_TAG="sox_ng-14.6.2"
git diff $PREVIOUS_TAG..$NEW_TAG -- src/sinc.c

# Update the patch in .flox/pkgs/sox_ng.nix to match new upstream code
$EDITOR .flox/pkgs/sox_ng.nix
```

### Large Sinc Filters Don't Work

If filters > 32767 taps fail:

```bash
# Verify patches are in Nix expression
grep "1073741823" .flox/pkgs/sox_ng.nix
# Should show 3 replacements

# Check build log for patch application
flox build sox_ng 2>&1 | grep -A5 "Large sinc filter"
# Should show: "Large sinc filter patches applied successfully!"
```

### Binary Still Named sox_ng

If the binary is named `sox_ng` instead of `sox`:

```bash
# Verify renaming patches are present
grep -A10 "sox_ng → sox Rebranding" .flox/pkgs/sox_ng.nix
# Should show extensive substituteInPlace commands

# Check postInstall creates correct symlinks
grep -A10 "postInstall" .flox/pkgs/sox_ng.nix
```

## Update Frequency

- sox_ng follows a **six-monthly release cadence**
- Check for updates every 3-6 months
- Subscribe to releases: https://codeberg.org/sox_ng/sox_ng/releases

## Version Numbering

- Upstream: `sox_ng-14.6.1.2` (from codeberg.org/sox_ng)
- Our version: `14.6.1.2-custom` (with `-custom` suffix)
- The `-custom` suffix indicates our patches are applied

## Files That Change During Update

When you run `git checkout sox_ng-14.6.2 -- ...`, these are updated:

- `src/` - All source files (100+ files)
- `configure.ac` - Autotools configuration
- `Makefile.am` - Build instructions
- `CMakeLists.txt` - CMake build configuration

## Files That DON'T Change (Our Custom Files)

These stay untouched during updates:

- `.flox/` - Our Nix expressions and Flox manifest
- `flake.nix` - Nix flake definition
- `flake.lock` - Locked dependencies
- `README.md` - Our custom README
- `UPSTREAM_TRACKING.md` - This documentation
- `EXTREME_RESAMPLING_WITH_SOX_A_GUIDE_FOR_THE_PERPLEXED.md`
- `LARGE_SINC_FILTERS_IMPLEMENTATION.md`
- `SOX_DSD_BASICS.md`
- `FIR_CHANGES_ANALYSIS.md`

## One-Liner Status Check

```bash
echo "Local: $(grep 'version =' .flox/pkgs/sox_ng.nix | head -1 | cut -d'"' -f2)" && \
echo "Upstream: $(git fetch upstream --tags 2>/dev/null && git tag -l 'sox_ng-*' --sort=-version:refname | head -1)"
```

## Emergency Rollback

If an update causes problems:

```bash
# View recent commits
git log --oneline -5

# Rollback to previous commit
git reset --hard HEAD~1

# Or rollback to specific commit
git reset --hard <commit-hash>

# Rebuild from previous version
flox build sox_ng
```

## Advanced: Testing on Multiple Platforms

If you have access to both Linux and macOS:

```bash
# On Linux
flox build sox_ng-linux  # Manifest build (Linux-specific)
flox build sox_ng         # Nix expression (cross-platform)

# On macOS
flox build sox_ng-darwin  # Manifest build (macOS-specific)
flox build sox_ng         # Nix expression (cross-platform)
```

The Nix expression build (`sox_ng`) should work on both platforms.

## Complete Update Example

Here's a real example from updating 14.6.1.2 → 14.6.2:

```bash
# 1. Check for updates
git fetch upstream --tags
git tag -l 'sox_ng-*' --sort=-version:refname | head -3
# Output:
# sox_ng-14.6.2
# sox_ng-14.6.1.2
# sox_ng-14.6.1.1

# 2. Checkout new version
git checkout sox_ng-14.6.2 -- src/ configure.ac Makefile.am CMakeLists.txt
git status
# Shows 78+ modified files in src/

# 3. Update Nix expression version
sed -i 's/version = "14.6.1.2-custom"/version = "14.6.2-custom"/' .flox/pkgs/sox_ng.nix

# Verify change
grep "version =" .flox/pkgs/sox_ng.nix | head -1
# Should show: version = "14.6.2-custom";

# 4. Test build
flox build sox_ng
# Wait for build...
# Should end with: ✨ Builds completed successfully

# 5. Verify functionality
./result-sox_ng/bin/sox --version
# Should show: SoX_ng v14.6.2

./result-sox_ng/bin/sox -n test.flac synth 1 sine 440
./result-sox_ng/bin/sox test.flac out.flac sinc -n 100000 -20000
ls -lh out.flac
# Should show output file created
rm test.flac out.flac

# 6. Test Nix flake build too
nix --extra-experimental-features 'nix-command flakes' build .
./result/bin/sox --version
# Should also show: SoX_ng v14.6.2

# 7. Update flake.lock (optional)
nix --extra-experimental-features 'nix-command flakes' flake update

# 8. Commit
git add -A
git commit -m "Update sox_ng source to sox_ng-14.6.2"

# 9. Push
git push origin main
```

## Questions?

For detailed explanations of why this workflow was chosen and how it works, see [UPSTREAM_TRACKING.md](UPSTREAM_TRACKING.md).

For general sox_ng documentation, see [README.md](README.md).

For extreme resampling parameters, see [EXTREME_RESAMPLING_WITH_SOX_A_GUIDE_FOR_THE_PERPLEXED.md](EXTREME_RESAMPLING_WITH_SOX_A_GUIDE_FOR_THE_PERPLEXED.md).
