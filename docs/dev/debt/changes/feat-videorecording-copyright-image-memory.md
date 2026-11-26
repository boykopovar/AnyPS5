# libSceVideoRecordingP

## remove [13.60 remaining inventory exports]

## add Functional

- [sceVideoRecordingGetCopyrightImageMemory](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) (libSceVideoRecordingP) - `sceVideoRecordingGetCopyrightImageMemory` signature, argument checks, null-pointer checks, and output size/address return values are implemented according to firmware 13.42 disassembly (`libSceVideoRecordingP.sprx` sha256 `6e51025a6e879039a03b5f9cecfdd33ff5fe20a67117f78c8952ca476df0218b`). When either pointer is null or inaccessible, returns `0x80a80003`; otherwise writes null pointer and size 0 to the outputs and returns `0x80a80001` when no copyright image is loaded.
