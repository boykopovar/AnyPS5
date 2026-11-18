# libSceVideoRecordingP

## change 13.60 remaining inventory exports

- [13.60 remaining inventory exports](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - `sceVideoRecordingCloseWithTruncationByHandle` (`oryBI-2iyxw`) has unknown signature and behavior. Parameterless `int` declaration is an import-resolution placeholder, not a verified ABI prototype; every call throws through `NotImplemented_nid_no_patch`. These defined name/NID bindings come from the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscevideorecordingp-2f2509cb1b.jsonl).

## add Functional

- [sceVideoRecordingGetCopyrightImageMemory](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) (libSceVideoRecordingP) - `sceVideoRecordingGetCopyrightImageMemory` signature, argument checks, null-pointer checks, and output size/address return values are implemented according to firmware 13.42 disassembly (`libSceVideoRecordingP.sprx` sha256 `6e51025a6e879039a03b5f9cecfdd33ff5fe20a67117f78c8952ca476df0218b`). When either pointer is null or inaccessible, returns `0x80a80003`; otherwise writes null pointer and size 0 to the outputs and returns `0x80a80001` when no copyright image is loaded.
