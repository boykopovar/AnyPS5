# libSceKbEmulate technical debt

### Unknown function info

- [Shared pad-emulation exports](../../../core/libs/prx/libSceKbEmulate/libSceKbEmulate.hpp) - `sceKeyboardPadEmulateClose` and `sceKeyboardPadEmulateOpen` are the complete named function inventory in the [PS5 13.60 export records](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscekbemulate-fda3ae4412.jsonl). They use the existing [libSceKeyboard throwing implementations](../../../core/libs/prx/libSceKeyboard/PadEmulate.cpp). Their signatures and behavior remain unverified; no keyboard-pad emulation is implemented.
