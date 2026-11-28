# libSceVideoCoreInterface technical debt

### Unknown function info

- The following pending declarations have unverified signatures and behavior; they are parameterless import-resolution declarations and throw through `NotImplemented_nid_no_patch`: [`sceVideoCoreIfMainLoop`](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp).

### Functional

- [sceVideoCoreIfInitializeInterface, sceVideoCoreIfSetFunctionTable and sceVideoCoreIfSetFunctionTableUnlimited](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp) (libSceVideoCoreInterface) - only the null checks firmware 13.42 makes before its first call are implemented, from its disassembly, not measured on a console: each returns `0x81793001` for a null first argument, and InitializeInterface also for a null second one. Past them they throw. Only the pointers these checks read are declared; their types and names are not from a header.
