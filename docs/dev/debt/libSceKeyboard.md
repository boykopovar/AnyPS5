# libSceKeyboard technical debt

### Functional

- [sceKeyboardGetKey2Char](../../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp): initialization, output length and reserved bytes remain unverified on a console; unconverted keys retain the host's length of 0. One title (PPSA15246) disassembled for this entry calls it after clearing the first 20 bytes of its output buffer (16-byte store 0x69942d, 32-bit store 0x699435; no size argument), uses a character only when the result is 0 and the first output byte is nonzero, and then reads the 16-bit value at offset 8. Which other output fields it reads and what its other arguments mean are not shown.

### Unknown function info

- [sceDbgKeyboardGetHandle](../../../core/libs/prx/libSceKeyboard/Export.cpp) (logical `libSceDbgKeyboard`, NID `vob3PZretTg`) - signature and behavior are unknown. This parameterless `int` declaration is an import placeholder, not a verified ABI prototype; every call throws through `NotImplemented_nid_no_patch`. The binding is in the [13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscedbgkeyboard-a009193dfc.jsonl).

- [sceDbgKeyboardClose, sceDbgKeyboardGetKey2Char, sceDbgKeyboardInit, sceDbgKeyboardOpen, sceDbgKeyboardRead, sceDbgKeyboardReadState](../../../core/libs/prx/libSceKeyboard/Export.cpp) (libSceKeyboard) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceKeyboard.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim.
- [sceKeyboardGetDeviceInfo and sceKeyboardSetProcessFocus](../../../core/libs/prx/libSceKeyboard/Export.cpp): pointer arguments are passed to device ioctls. Device-info storage is 168 bytes in the connection-query caller, but its fields and the process-focus layout remain unresolved.
- [sceKeyboardConnectPort, sceKeyboardDisconnectPort and sceKeyboardDeviceOpen](../../../core/libs/prx/libSceKeyboard/Export.cpp): argument widths are confirmed. The meaning/order of the two 32-bit port arguments and the final 32-bit device-open argument still require the device-service backend.
- [sceKeyboardPadEmulateClose and sceKeyboardPadEmulateOpen](../../../core/libs/prx/libSceKeyboard/Export.cpp): the supplied keyboard library has neither export; their signatures remain unresolved.
