# libSceNpGameIntent technical debt

### Unknown function info

- [sceNpAppLauncherLaunchApp](../../../core/libs/prx/libSceNpGameIntent/Export.cpp) (libSceNpGameIntent, NID `GbayQ7DO8jA`) - its recovered signature is `(int32_t, const char*, const char*)`: an unhandled first argument, a required non-empty string and an optional string rejected past 0x2000 bytes, returning `0x80553A02` before `sceNpAppLauncherInitialize` and `0x80553A04` on invalid arguments. The launch itself is not implemented and throws; the firmware tail calls an internal launch through an extra library context and remaps its `0x805538xx` errors into `0x80553Axx`
