# libSceAgcDriver

## add Functional

- [Expanded HTILE stores over native multisample depth](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - a GPU store over a native multisample depth target's HTILE plane is accepted when, once it lands, every HTILE word is already expanded (`0xfffc000f` for depth-only HTILE): the native image keeps its depth values. On the console, resetting HTILE that held compressed tiles to expanded words exposes the depth plane's raw bytes for those tiles; the host keeps the native depth values instead, which differs only if the game reads depth it reset this way without rewriting it (not measured). Resident Evil Village (PPSA01556) stores whole HTILE planes this way. GPU stores of other HTILE words, and any GPU store over the depth or stencil plane (including the stencil plane of a D32S8 target without HTILE), still throw.
