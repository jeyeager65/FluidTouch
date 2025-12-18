# FluidTouch Web Installer - Version Management

## Overview

The FluidTouch web installer supports multiple firmware versions with dropdown selection. This allows users to choose between stable releases, preview builds, and different hardware variants.

## Directory Structure

```
web/
├── firmware/
│   ├── {version}/              # Stable release versions
│   │   ├── basic/
│   │   │   ├── firmware.bin
│   │   │   ├── bootloader.bin
│   │   │   ├── partitions.bin
│   │   │   └── boot_app0.bin
│   │   └── advance/
│   │       ├── firmware.bin
│   │       ├── bootloader.bin
│   │       ├── partitions.bin
│   │       └── boot_app0.bin
│   └── preview/
│       └── {version_branchname}/   # Preview builds from feature branches
│           ├── basic/
│           └── advance/
├── versions.json               # Version registry
├── manifest_basic.json         # Legacy manifest (for local/basic builds)
└── manifest_advance.json       # Legacy manifest (for local/advance builds)
```

## versions.json Structure

```json
{
  "default_version": "1.0.2",
  "default_hardware": "advance-v1.3",
  "versions": [
    {
      "id": "1.0.2",
      "hardware": ["basic", "advance-v1.3"],
      "is_preview": false
    },
    {
      "id": "1.0.2_status-tab-updates",
      "hardware": ["basic", "advance-v1.3"],
      "is_preview": true
    }
  ],
  "hardware_info": {
    "basic": "Basic (4MB flash, PWM backlight)",
    "advance-v1.3": "Advance v1.3 (16MB flash, IPS display)",
    "advance-v1.2": "Advance v1.2 (older revision)"
  }
}
```

## Local Development

### Build Commands

```powershell
# Default local build (legacy path: web/firmware/basic/, web/firmware/advance/)
.\scripts\build-web-installer.ps1

# Versioned stable release
.\scripts\build-web-installer.ps1 -Version "1.0.2" -UpdateVersions

# Preview build with automatic branch name
.\scripts\build-web-installer.ps1 -Preview -UpdateVersions

# Clean build
.\scripts\build-web-installer.ps1 -Clean -Version "1.0.2" -UpdateVersions

# Start local test server
.\scripts\build-web-installer.ps1 -StartServer
```

### Testing Locally

1. Build firmware with version:
   ```powershell
   .\scripts\build-web-installer.ps1 -Version "1.0.2-test" -UpdateVersions
   ```

2. Start local server:
   ```powershell
   .\scripts\build-web-installer.ps1 -StartServer
   ```

3. Open browser to `http://localhost:8000`

4. Select version and hardware from dropdowns

5. Connect ESP32-S3 device and flash

## GitHub Actions Workflows

### Stable Releases (build-firmware.yml)

- **Trigger**: Git tags matching `v*.*.*` (e.g., `v1.0.2`)
- **Output**: `firmware/{version}/basic/` and `firmware/{version}/advance/`
- **versions.json**: Prepends new version, sets as default
- **Deployment**: GitHub Pages at root

### Preview Builds (build-branch-preview.yml)

- **Trigger**: Pushes to `feature/**`, `fix/**`, `dev` branches
- **Version ID**: `{base_version}_{branch_suffix}` (e.g., `1.0.2_status-tab-updates`)
- **Output**: `firmware/preview/{branch_suffix}/basic/` and `firmware/preview/{branch_suffix}/advance/`
- **versions.json**: Replaces existing preview with same ID
- **Deployment**: GitHub Pages at root

## Manifest Paths

The web installer dynamically loads manifests based on selection:

```
firmware/{version}/manifest_basic.json
firmware/{version}/manifest_advance.json
firmware/preview/{version}/manifest_basic.json
firmware/preview/{version}/manifest_advance.json
```

**Note**: Manifests are currently **NOT** generated automatically. They must be copied or created manually in each version folder if needed. The current implementation uses hardcoded manifest references in `index.html`.

## Version Lifecycle

1. **Development**: Work on feature branch
   - Preview builds deploy automatically on push
   - Accessible via dropdown with orange "(Preview)" badge

2. **Release**: Create git tag
   - Stable build deploys automatically
   - Added to versions.json as default
   - Preview for same base version remains available

3. **Rollback**: Users can select older versions
   - All versions preserved indefinitely
   - No automatic cleanup

## Hardware Variants

### Current Variants

- **basic**: Elecrow CrowPanel 7" Basic (4MB flash, PWM backlight)
- **advance-v1.3**: Elecrow CrowPanel 7" Advance v1.3 (16MB flash, IPS display)
- **advance-v1.2**: Elecrow CrowPanel 7" Advance v1.2 (older revision) - **Not yet implemented**

### Adding New Hardware

1. Add variant to `hardware_info` in versions.json
2. Update workflows to build new variant
3. Update `index.html` hardware descriptions
4. Create manifest for new hardware

## Fallback Behavior

If `versions.json` fails to load or is missing:

- Web installer falls back to legacy mode
- Uses root-level `manifest_basic.json` and `manifest_advance.json`
- Hardware cards remain functional
- No version selection available

## Troubleshooting

### Version not appearing in dropdown

- Check versions.json syntax (valid JSON)
- Ensure version entry has required fields: `id`, `hardware`, `is_preview`
- Verify firmware files exist at expected paths

### Manifest 404 errors

- Verify version folder structure matches dropdown paths
- Check that manifest files exist in version directories
- Inspect browser console for exact failed path

### Preview not marked as preview

- Ensure version ID contains underscore (e.g., `1.0.2_branch`)
- Or set `is_preview: true` explicitly in versions.json

### Local server CORS issues

- Use `python -m http.server 8000` or equivalent
- Do NOT use `file://` protocol
- ESP Web Tools requires HTTP/HTTPS
