# libSceVideoOut

## add Host screen keep-awake

- [Host screen keep-awake](../../../core/libs/prx/libSceVideoOut/src/VideoOutDriver.cpp) (libSceVideoOut) - the host screen is kept awake by SDL's own default, which disables the screen saver when its video subsystem starts; no host where the screen still blanks with that default is named or measured, and Windows, X11, Wayland and macOS were not run here. `ANYPS5_KEEP_AWAKE=0` sets `SDL_VIDEO_ALLOW_SCREENSAVER=1`, which lets the screen saver run; that is not measured on any host either.
