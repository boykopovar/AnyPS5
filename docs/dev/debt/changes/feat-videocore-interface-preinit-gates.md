# libSceVideoCoreInterface

## change [sceVideoCoreIfInitializeInterface, sceVideoCoreIfSetFunctionTable and sceVideoCoreIfSetFunctionTableUnlimited](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp) (libSceVideoCoreInterface)

- [sceVideoCoreIfInitializeInterface, sceVideoCoreIfSetFunctionTable, sceVideoCoreIfSetFunctionTableUnlimited and sceVideoCoreIfMainLoop](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp) (libSceVideoCoreInterface) - from firmware 13.60 `libSceVideoCoreInterface.sprx` (SHA-256 `f03fe1565a781ac2a5a08a568e481df174734e28c9233e1b830130ed38bd1252`, disassembly only): `SetFunctionTable` (`0x80a0`), `SetFunctionTableUnlimited` (`0x80e0`), and `MainLoop` (`0x8120`) return `0x81793002` when uninitialized (after null checks on `SetFunctionTable*`); `InitializeInterface` still throws past its null checks (`0x81793001`). Only the pointers these checks read are declared.

## change [`sceVideoCoreIfMainLoop`](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp)

- The following pending declarations have unverified signatures and behavior; they are parameterless import-resolution declarations (`sceVideoCoreIfMainLoop` returns `0x81793002` before initialization as noted under Functional): [`sceVideoCoreIfMainLoop`](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp).
