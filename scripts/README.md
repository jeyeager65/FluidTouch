# Script Utilities

## Keep developer updates + working driver fixes (cross-platform)

These scripts automate the safe workflow for keeping your custom driver branch up to date with upstream `main`.

- **Windows PowerShell**: `Sync-DriverBranch.ps1`
- **Linux/macOS Bash**: `sync-driver-branch.sh`

What they do:

1. Optionally stash dirty local changes
2. Fetch and fast-forward `main` from remote
3. Switch to your driver branch
4. Create a timestamped safety backup branch
5. Rebase (default) or merge from `main`
6. Optionally push updated branches
7. Restore your original branch and stashed changes

Defaults:

- main branch: `main`
- driver branch: `waveshare-driver-stable`
- remote: `origin`
- integration mode: rebase

### PowerShell examples

- Default rebase workflow: `./Sync-DriverBranch.ps1`
- Merge instead of rebase: `./Sync-DriverBranch.ps1 -Merge`
- Also push after sync: `./Sync-DriverBranch.ps1 -Push`

### Bash examples

- Default rebase workflow: `./sync-driver-branch.sh`
- Merge instead of rebase: `./sync-driver-branch.sh --merge`
- Also push after sync: `./sync-driver-branch.sh --push`

> Note: On Linux/macOS, make executable once with `chmod +x scripts/sync-driver-branch.sh`.

# Custom LVGL Font Generation

This directory contains scripts for generating custom LVGL fonts from FontAwesome icons.

## Prerequisites

- **Node.js** (v14 or higher) - [Download](https://nodejs.org/)
- **lv_font_conv** - Automatically installed on first run

## Usage

### Quick Start (Recommended)

Regenerate all fonts from the configuration file:

```powershell
.\Generate-LVGLFont.ps1 -All
```

### Generate Specific Font from Config

```powershell
.\Generate-LVGLFont.ps1 -FontName "fontawesome_icons_20"
```

### Manual Generation (Without Config)

```powershell
.\Generate-LVGLFont.ps1 -FontName "fontawesome_icons_20" -FontSize 20 -Icons @{lock="0xf023"; unlock="0xf09c"}
```

## Configuration File

The `fonts.json` file stores all font definitions for easy regeneration:

```json
{
  "fonts": [
    {
      "name": "fontawesome_icons_20",
      "size": 20,
      "icons": {
        "lock": "0xf023",
        "unlock": "0xf09c"
      },
      "description": "FontAwesome lock/unlock icons for WCS settings"
    }
  ]
}
```

To add new fonts, simply add entries to the `fonts` array in `fonts.json`.

### Parameters

- **`-FontName`** (Required): Name of the output font file (without extension)
- **`-FontSize`** (Required): Font size in pixels
- **`-Icons`** (Required): Hashtable of icon names and Unicode codepoints
- **`-FontFile`** (Optional): Path to custom font file (defaults to FontAwesome Free Solid)

### Finding Icon Codepoints

1. Visit [FontAwesome Icons](https://fontawesome.com/icons)
2. Search for the icon you want
3. Click on the icon to see its details
4. Look for the Unicode codepoint (e.g., `f023` for lock)
5. Prefix with `0x` (e.g., `0xf023`)

### Examples

**Lock and Unlock Icons:**
```powershell
.\Generate-LVGLFont.ps1 -FontName "fontawesome_icons_20" -FontSize 20 -Icons @{
    lock = "0xf023"
    unlock = "0xf09c"
}
```

**Multiple Icons:**
```powershell
.\Generate-LVGLFont.ps1 -FontName "fontawesome_icons_24" -FontSize 24 -Icons @{
    lock = "0xf023"
    unlock = "0xf09c"
    home = "0xf015"
    settings = "0xf013"
    wifi = "0xf1eb"
}
```

**Custom Font File:**
```powershell
.\Generate-LVGLFont.ps1 -FontName "custom_icons" -FontSize 18 -Icons @{lock="0xf023"} -FontFile "C:\Fonts\my-font.ttf"
```

## Output

The script generates two files in `src/ui/fonts/`:

1. **`<FontName>.c`** - LVGL font data (C source file)
2. **`<FontName>.h`** - Header with icon defines and font declaration

### Using Generated Icons in Code

```cpp
#include "ui/fonts/fontawesome_icons_20.h"

// Create a label with lock icon
lv_obj_t *lbl = lv_label_create(parent);
lv_label_set_text(lbl, FA_ICON_LOCK);
lv_obj_set_style_text_font(lbl, &fontawesome_icons_20, 0);

// Change to unlock icon
lv_label_set_text(lbl, FA_ICON_UNLOCK);
```

## Cache Directory

FontAwesome fonts are cached in `scripts/fontawesome-cache/` to avoid repeated downloads.

## Troubleshooting

### "Node.js is not installed"
Install Node.js from https://nodejs.org/

### "Font generation failed"
- Ensure the codepoint is valid (check FontAwesome website)
- Try with a different font size
- Check that the font file exists

### "lv_font_conv: command not found"
The script will automatically install lv_font_conv on first run. If it fails, install manually:
```powershell
npm install -g lv_font_conv
```

## FontAwesome Resources

- **Icon Search**: https://fontawesome.com/icons
- **Free Icons**: https://fontawesome.com/search?o=r&m=free
- **Cheatsheet**: https://fontawesome.com/cheatsheet

## Common Icons

| Icon | Name | Codepoint |
|------|------|-----------|
| 🔒 | lock | 0xf023 |
| 🔓 | unlock | 0xf09c |
| 🏠 | home | 0xf015 |
| ⚙️ | settings | 0xf013 |
| 📡 | wifi | 0xf1eb |
| 🔍 | search | 0xf002 |
| ⬇️ | download | 0xf019 |
| ⬆️ | upload | 0xf093 |
| ✓ | check | 0xf00c |
| ✕ | times | 0xf00d |
| ℹ️ | info | 0xf129 |
| ⚠️ | warning | 0xf071 |
| 🗑️ | trash | 0xf1f8 |
| ✏️ | edit | 0xf044 |
| 💾 | save | 0xf0c7 |

## License

This script uses FontAwesome Free, which is licensed under CC BY 4.0 and SIL OFL 1.1.
See https://fontawesome.com/license/free for details.
