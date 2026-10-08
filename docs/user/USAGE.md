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