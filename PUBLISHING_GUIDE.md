# Publishing Guide for sox_ng

This guide covers how to publish sox_ng packages to Flox catalogs using the built-in helper functions.

## Prerequisites

Before publishing, ensure:

1. **Flox authentication**: `flox auth login`
2. **Clean git state**: No uncommitted changes
3. **Latest commit pushed**: `git push origin main`
4. **Versions synchronized**: All builds at 14.6.1.2-custom

## Quick Start

```bash
# Activate environment to load helpers
flox activate

# Check what can be published
list_publishable

# Verify you're ready
check_publish_ready

# Publish to your namespace
publish_sox barstoolbluz sox_ng-linux
```

## Available Helper Functions

All functions are available after `flox activate`:

### Information & Validation

#### `list_publishable`
Shows all publishable packages with versions.

```bash
$ list_publishable
═══════════════════════════════════════════════════════════════
Publishable sox_ng Packages
═══════════════════════════════════════════════════════════════

Manifest Builds (platform-specific):
  • sox_ng-linux  (14.6.1.2-custom) - Linux x86_64/aarch64
  • sox_ng-darwin (14.6.1.2-custom) - macOS x86_64/aarch64

Nix Expression Build (cross-platform):
  • sox_ng        (14.6.1.2-custom) - Via .flox/pkgs/sox_ng.nix
```

#### `check_publish_ready`
Validates all prerequisites before publishing.

```bash
$ check_publish_ready
═══════════════════════════════════════════════════════════════
Pre-Publish Validation
═══════════════════════════════════════════════════════════════

✓ Git status: Clean working tree
✓ Commits: All pushed to remote
✓ Versions: Synchronized (14.6.1.2-custom)
✓ Auth: Logged in as barstoolbluz

✓ Ready to publish!
```

### Publishing Functions

#### `publish_sox <org> <package>`
Main publishing function with full validation.

**Parameters:**
- `<org>`: Organization name (e.g., `barstoolbluz`, `flox`, `myorg`)
- `<package>`: Package name (`sox_ng-linux`, `sox_ng-darwin`, or `sox_ng`)

**Examples:**
```bash
# Publish to your namespace
publish_sox barstoolbluz sox_ng-linux

# Publish to an organization
publish_sox myorg sox_ng-darwin

# Publish Nix expression build
publish_sox barstoolbluz sox_ng
```

**Workflow:**
1. Validates arguments
2. Runs pre-flight checks
3. Prompts for confirmation
4. Executes `flox publish -o <org> <package>`
5. Shows result

#### `publish_to_flox <package>`
Shortcut to publish to the `flox` organization.

**Examples:**
```bash
publish_to_flox sox_ng-linux
publish_to_flox sox_ng-darwin
publish_to_flox sox_ng
```

**Note**: Requires write access to the `flox` organization.

#### `publish_all <org>`
Publishes all three packages to one organization.

**Parameters:**
- `<org>`: Organization name (defaults to `barstoolbluz` if omitted)

**Examples:**
```bash
# Publish all to your namespace
publish_all barstoolbluz

# Publish all to organization
publish_all myorg

# Use default (barstoolbluz)
publish_all
```

**Workflow:**
1. Lists all three packages
2. Prompts for confirmation
3. Publishes sequentially: sox_ng-linux, sox_ng-darwin, sox_ng
4. Stops on failure (with option to continue)
5. Shows summary

#### `show_published <org>`
Searches for published sox_ng packages.

**Parameters:**
- `<org>`: Organization to search (defaults to `barstoolbluz`)

**Examples:**
```bash
show_published barstoolbluz
show_published flox
```

### Help

#### `sox_publish_help`
Shows quick usage summary.

```bash
$ sox_publish_help
═══════════════════════════════════════════════════════════════
Sox_ng Publishing Helper Functions
═══════════════════════════════════════════════════════════════

Available commands:
  list_publishable              - List all publishable packages
  check_publish_ready           - Validate prerequisites
  publish_sox <org> <pkg>       - Publish to organization
  publish_to_flox <pkg>         - Publish to flox org
  publish_all <org>             - Publish all packages
  show_published <org>          - Search published packages
  sox_publish_help              - Show this help
```

## Three Packages Explained

### 1. sox_ng-linux (Manifest Build)
- **Location**: `.flox/env/manifest.toml` `[build.sox_ng-linux]`
- **Platforms**: Linux x86_64, aarch64
- **Build**: Manifest build with pure sandbox
- **Publish**: `publish_sox <org> sox_ng-linux`

### 2. sox_ng-darwin (Manifest Build)
- **Location**: `.flox/env/manifest.toml` `[build.sox_ng-darwin]`
- **Platforms**: macOS x86_64, aarch64
- **Build**: Manifest build with platform-specific fixes
- **Publish**: `publish_sox <org> sox_ng-darwin`

### 3. sox_ng (Nix Expression)
- **Location**: `.flox/pkgs/sox_ng.nix`
- **Platforms**: Cross-platform (Nix handles all)
- **Build**: Nix expression with full reproducibility
- **Publish**: `publish_sox <org> sox_ng`

## Publishing Workflows

### Workflow 1: Publish Single Package

```bash
# 1. Activate environment
flox activate

# 2. Verify readiness
check_publish_ready

# 3. Publish
publish_sox barstoolbluz sox_ng-linux
```

### Workflow 2: Publish to Multiple Organizations

```bash
flox activate

# Publish to your namespace
publish_sox barstoolbluz sox_ng-linux

# Publish to team organization
publish_sox myteam sox_ng-linux

# Publish to flox (if you have access)
publish_to_flox sox_ng-linux
```

### Workflow 3: Publish All Packages

```bash
flox activate

# Option A: To your namespace
publish_all barstoolbluz

# Option B: To flox organization
publish_all flox
```

### Workflow 4: After Updating Source

```bash
# 1. Update source to new version (see QUICK_UPDATE_GUIDE.md)
git checkout sox_ng-14.6.2 -- src/ configure.ac Makefile.am CMakeLists.txt

# 2. Update versions in manifest and Nix expression
# (Edit .flox/env/manifest.toml and .flox/pkgs/sox_ng.nix)

# 3. Test builds
flox build sox_ng-linux
flox build sox_ng

# 4. Commit and push
git add -A
git commit -m "Update to sox_ng 14.6.2"
git push origin main

# 5. Activate and publish
flox activate
check_publish_ready
publish_all barstoolbluz
```

## Troubleshooting

### Error: "Uncommitted changes detected"
```bash
# Solution: Commit everything
git add -A
git commit -m "Prepare for publish"
git push origin main
```

### Error: "Unpushed commits"
```bash
# Solution: Push to remote
git push origin main
```

### Error: "Version mismatch"
This means versions in different files are out of sync.

**Check versions:**
```bash
grep "AC_INIT" configure.ac                    # Source
grep "version =" .flox/pkgs/sox_ng.nix         # Nix expression
grep 'version = "14' .flox/env/manifest.toml   # Manifest builds
```

**Fix**: Update all to match current version.

### Error: "Not logged in"
```bash
# Solution: Authenticate
flox auth login
```

### Error: "Permission denied" for organization
You don't have write access to that organization.

**Solutions:**
- Publish to your personal namespace instead
- Request access from organization admin
- Use a different organization

### Error: "Build failed" during publish
Flox does a clean build in isolation. If it fails:

1. Check build works locally: `flox build <package>`
2. Ensure all build files are committed
3. Review build logs from Flox publish output
4. Fix issues and retry

## Organization vs Personal Namespace

### Personal Namespace
- **Name**: Your username (e.g., `barstoolbluz`)
- **Access**: Only you
- **Use for**: Personal projects, testing
- **Publish**: `publish_sox barstoolbluz sox_ng-linux`

### Organization Catalog
- **Name**: Organization name (e.g., `flox`, `myteam`)
- **Access**: Organization members (paid feature)
- **Use for**: Team sharing, official releases
- **Publish**: `publish_sox flox sox_ng-linux`

## After Publishing

Once published, packages are available as:
- `barstoolbluz/sox_ng-linux`
- `barstoolbluz/sox_ng-darwin`
- `barstoolbluz/sox_ng`

**Users can install with:**
```bash
flox install barstoolbluz/sox_ng-linux
flox install barstoolbluz/sox_ng-darwin
flox install barstoolbluz/sox_ng
```

**Search for packages:**
```bash
flox search barstoolbluz/sox
```

## Tips & Best Practices

1. **Always run `check_publish_ready` first** - Catches issues early
2. **Test builds locally before publishing** - Though Flox will test anyway
3. **Use meaningful version numbers** - Current: `14.6.1.2-custom`
4. **Publish to personal namespace first** - Test before publishing to orgs
5. **Document what's changed** - In commit messages and README
6. **Keep versions synchronized** - All builds should match source version
7. **Push before publishing** - Flox clones from git remote

## Integration with Update Workflow

When updating from upstream sox_ng:

```bash
# 1. Update source (see QUICK_UPDATE_GUIDE.md)
./QUICK_UPDATE_GUIDE.md steps...

# 2. Update ALL version numbers
#    - configure.ac
#    - .flox/pkgs/sox_ng.nix
#    - .flox/env/manifest.toml (both sox_ng-linux and sox_ng-darwin)

# 3. Test all builds
flox build sox_ng-linux
flox build sox_ng-darwin  # If on macOS
flox build sox_ng

# 4. Commit, push, publish
git add -A
git commit -m "Update to sox_ng X.Y.Z"
git push origin main
flox activate
publish_all barstoolbluz
```

## See Also

- [QUICK_UPDATE_GUIDE.md](QUICK_UPDATE_GUIDE.md) - Updating to new sox_ng versions
- [UPSTREAM_TRACKING.md](UPSTREAM_TRACKING.md) - Tracking upstream releases
- [BUILDING_INTERMEDIATE_VERSIONS.md](BUILDING_INTERMEDIATE_VERSIONS.md) - Building older versions
- [FLOX.md](FLOX.md) - Complete Flox documentation (§11 Publishing)
