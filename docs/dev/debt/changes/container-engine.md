# TechnicalDebt

## add Build

- [Windows container build](../../../tools/engine/Dockerfile) - the Ubuntu MinGW compiler cannot link the required System V exception code. The builder runs the pinned Windows WinLibs compiler and build tools under Wine. A staged patch replaces libusb's PowerShell version-header command with a portable CMake generator; the original submodule remains unchanged. Closes when: a verified native Linux cross compiler supports the required ABI and the pinned libusb build supports a portable metadata generator.
