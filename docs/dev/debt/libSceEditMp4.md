# libSceEditMp4 technical debt

### Silent stubs

- [sceEditMp4QueryMemSize](../../../core/libs/prx/libSceEditMp4/Export.cpp) (libSceEditMp4) - returns 0x387d30 and reads no argument, as firmware 13.42's libSceEditMp4.sprx does (read from its disassembly and run natively on x86-64, not a console): the value is the sum of four internal functions that return constants and 0x980. Whether the public prototype has unused parameters is not established.

### Unknown function info

- [sceEditMp4Abort, sceEditMp4Edit, sceEditMp4GetStatus, sceEditMp4Initialize, sceEditMp4SetFinishCallback, sceEditMp4Terminate and sceEditMp4Unite](../../../core/libs/prx/libSceEditMp4/Export.cpp) (libSceEditMp4) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceEditMp4.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [sceEditMp4InitializeParam](../../../core/libs/prx/libSceEditMp4/Export.cpp) (libSceEditMp4) - the size of the parameter structure and the meaning of its fields are unknown. It follows firmware 13.42's libSceEditMp4.sprx, read from its disassembly and run natively on x86-64 (not a console): it returns nothing, ignores a null pointer and otherwise clears bytes 0 to 15, stores the 64-bit value 0x3f at offset 16 and the 32-bit value 700 at offset 24, and leaves the bytes from offset 28 unchanged.
