# AnyPS5 Usage Guide

This guide covers how to use AnyPS5, from basic setup to advanced configuration.

## Quick Start

1. Download the latest release or build from source
2. Place your PS5 game ISO files in a folder
3. Run AnyPS5 and load your first game!

## Loading Games

AnyPS5 supports loading games from:
- **ISO files**: Standard PS5 disc image format
- **FS0 directories**: Extracted game data folders

To load a game, use the command line or the built-in file browser (when available).

## Configuration

AnyPS5 uses JSON configuration files for settings. The main config file is `anyps5.json` in your working directory.

### Key Settings

- **GPU backend**: Choose between Vulkan and other backends
- **Resolution scaling**: Adjust internal rendering resolution
- **Audio device**: Select output audio device
- **Controller mapping**: Configure input devices

## System Fonts

Many PS5 games use the console's system fonts for text display. Without proper font files, games may show missing or incorrect text.

### Why Fonts Matter

PS5 games expect specific system fonts to be available. AnyPS5 needs these fonts to render text correctly in menus, subtitles, and in-game UI elements.

### Setting Up Fonts (Step-by-Step)

**Option 1: Use Console-Dumped Fonts (Best Quality)**

If you have access to a PS5 console, you can dump the system fonts:

1. Create a folder named `anyps5-fonts/` next to your AnyPS5 executable
2. Copy the following font files from your PS5 into this folder:
   - `SST-Roman.otf` (standard text)
   - `SST-Bold.otf` (bold text)
   - `SSTJpPro-Regular.otf` (Japanese characters)
   - Any other SST fonts you find on your console

**Option 2: Use Open-Source Substitutes (Easier)**

If you don't have a PS5 to dump fonts from, you can use freely available alternatives:

1. Create a folder named `anyps5-fonts/` next to your AnyPS5 executable
2. Download and place these font files in the folder:
   - **Latin/Vietnamese**: Noto Sans family (Light, Regular, Medium, Bold weights)
     - Files: `NotoSans-Light.ttf`, `NotoSans-Regular.ttf`, `NotoSans-Medium.ttf`, `NotoSans-Bold.ttf`
     - Also include italic variants if available
   - **Monospace**: Noto Sans Mono family (Light, Regular, Medium, Bold)
     - Files: `NotoSansMono-Light.ttf`, etc.
   - **Thai**: Noto Sans Thai family
   - **Japanese/Chinese**: Noto Sans CJK family

You can download these fonts from [Google Fonts](https://fonts.google.com/) or your Linux distribution's package manager.

### Custom Font Directory

To use a different location for font files, set the environment variable:
```bash
export ANYPS5_SYSTEM_FONTS=/path/to/your/fonts
```

### Troubleshooting Font Issues

**Problem**: Games show no text or blank boxes
- **Solution**: Verify that your `anyps5-fonts/` folder exists and contains font files
- Check the AnyPS5 log for font loading errors

**Problem**: Japanese/Chinese characters display incorrectly
- **Solution**: Ensure you have Noto Sans CJK fonts installed in your fonts directory

**Problem**: Text looks different from PS5
- **Solution**: Console-dumped SST fonts will match exactly; substitute fonts may have slightly different metrics

## Troubleshooting

### Game won't start
- Verify the ISO file is not corrupted
- Check that you have sufficient disk space for shader cache
- Try updating to the latest AnyPS5 version

### Poor performance
- Lower resolution scaling settings
- Disable unnecessary features like motion blur
- Ensure your GPU drivers are up to date

## Advanced Topics

### Shader Cache
AnyPS5 compiles shaders on first use and caches them. This can cause initial stuttering but improves over time. The cache is stored in `anyps5-shader-cache/`.

### Debug Logging
Enable verbose logging by adding `--log-level=debug` to your command line arguments. Logs are written to `anyps5.log`.

## System Requirements

- **OS**: Windows 10+, Linux, or macOS
- **CPU**: Modern multi-core processor (AVX2 recommended)
- **GPU**: Vulkan-compatible graphics card
- **RAM**: 8GB minimum, 16GB recommended