# libScePad

## add Unknown function info

- [scePadResetOrientationForTracker](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - the `(int handle)` signature and resetting the controller's orientation like `scePadResetOrientation` follow the `scePadSetLightBarForTracker` precedent in the same file, where the tracker variant delegates to the plain setter. How the console's tracker path differs from a plain orientation reset is not measured. Closes when: measured on a PS5
