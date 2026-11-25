# libSceVdecSavc

## change [sceSdecDestroySw]

- [sceSdecDestroySw](../../../core/libs/prx/libSceVdecSavc/Export.cpp) (libSceVdecSavc) - clears the codec (`+0x10`) and workspace (`+0x08`) pointers and clears an attached sub-codec slot (`+0x30` on the codec object) via checked guest reads/writes while preserving `errno`, matching firmware 13.60 `libSceVdecSavc.sprx` disassembly (`0x530`, `0x16a0`, not measured on a console); full internal software codec destruction and unreadable-pointer fault semantics remain unimplemented.

## add Functional

- [sceSdecCreateSw](../../../core/libs/prx/libSceVdecSavc/Export.cpp) (libSceVdecSavc) - initializes the 24-byte handle header, 32-byte workspace bump-allocator header, 0x140-byte codec descriptor, and optional 0x3fca0-byte sub-codec reservation inside caller-supplied memory, storing the handle at `*handle` and returning `0`, following firmware 13.60 `libSceVdecSavc.sprx` disassembly (`0x300`, `0x760`, `0x1610`, `0x1710`, not measured on a console); H.264 stream decode execution beyond handle construction remains unimplemented.
