# libSceKeyboard

## change [sceKeyboardPadEmulateClose and sceKeyboardPadEmulateOpen]

- [sceKeyboardPadEmulateClose and sceKeyboardPadEmulateOpen](../../../core/libs/prx/libSceKeyboard/PadEmulate.cpp): firmware 13.42's `libSceKeyboard.sprx` has neither export; `libSceKbEmulate.sprx` has both ([reading](https://github.com/boykopovar/AnyPS5/issues/21849#issuecomment-6939482475), disassembly only, not run on a console). `sceKeyboardPadEmulateOpen(void* p)` returns `0x80BC0022` for a null `p` and `0x80BC0001` when already open, then connects to a system service over IPMI and starts an event thread; that isn't implemented, so a non-null `p` throws. `sceKeyboardPadEmulateClose(void)` returns `0x80BC0002` when not open, as it never is here. What `p` points to wasn't read

# libSceKbEmulate

## change [Shared pad-emulation exports]

- [Shared pad-emulation exports](../../../core/libs/prx/libSceKbEmulate/libSceKbEmulate.hpp) - `sceKeyboardPadEmulateClose` and `sceKeyboardPadEmulateOpen` are the complete named function inventory in the [PS5 13.60 export records](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscekbemulate-fda3ae4412.jsonl). They use the [libSceKeyboard implementations](../../../core/libs/prx/libSceKeyboard/PadEmulate.cpp), whose entry gives firmware 13.42's checks; no keyboard-pad emulation is implemented.
