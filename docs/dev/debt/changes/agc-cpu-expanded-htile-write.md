# libSceAgcDriver

## add Functional

- [CPU writes of expanded HTILE words over native multisample depth](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - not measured on the console: after a guest CPU write of expanded HTILE words (`0xfffc000f`, depth-only) the console would expose the depth plane's raw bytes for tiles that held compressed data, while the host keeps the native depth values; this differs only if the game reads such depth without rewriting it. The write is accepted when the depth and stencil planes are unchanged and every HTILE word is expanded. Resident Evil Village (PPSA01556) writes whole HTILE planes of expanded words this way while loading its first level. Other CPU HTILE writes, and CPU writes over the depth or stencil plane, still throw.
