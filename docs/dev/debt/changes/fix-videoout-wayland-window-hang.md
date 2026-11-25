# libSceVideoOut

## add Functional

- [Wayland window decorations](../../../core/libs/prx/libSceVideoOut/src/DisplayWindow.cpp) - a window can lack a title bar and borders if libdecor fails or the compositor declines server-side decorations. The [build-time patch](../../../cmake/PatchSdlWayland.cmake) adds a configure timeout and xdg-shell retry to [SDL2 74be4c2836da0940f0144d0d97852606f9ab11e9](https://github.com/libsdl-org/SDL/tree/74be4c2836da0940f0144d0d97852606f9ab11e9). Builds with `ANYPS5_USE_SYSTEM_DEPS=ON` use the system SDL2's Wayland behavior.
