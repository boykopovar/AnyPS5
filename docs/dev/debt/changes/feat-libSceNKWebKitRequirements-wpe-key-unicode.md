# libSceNKWebKitRequirements

## add Functional

- [wpe_key_code_to_unicode, wpe_unicode_to_key_code](../../../core/libs/prx/libSceNKWebKitRequirements/Wpe.cpp) (libSceNKWebKitRequirements, NIDs `YFlXC+lRh0A`, `uVoRmKYYtcI`) have no console verification, and the console's libwpe version is unknown. They run [`src/key-unicode.c`](https://github.com/WebPlatformForEmbedded/libwpe/blob/b90a8396864c9824a43838a5879f6ab01420c69e/src/key-unicode.c) of libwpe 1.14.0 from `3rdparty/libwpe`, the minimum the [PS5 WebKit 11.x build](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/cmake/OptionsPlayStation.cmake#L103) asks for; libwpe 1.14.0 to 1.16.3 ship the same file. The `uint32_t` prototypes follow libwpe's `input.h`. `uVoRmKYYtcI` is in the 3.20 inventory but not in the 13.60 export table.
