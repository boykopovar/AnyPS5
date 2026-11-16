# libSceEditMp4 technical debt

### Silent stubs

- [sceEditMp4QueryMemSize](../../../core/libs/prx/libSceEditMp4/Export.cpp) (libSceEditMp4) - returns 0x387d30 and reads no argument, as firmware 13.42's libSceEditMp4.sprx does (read from its disassembly and run natively on x86-64, not a console): the value is the sum of four internal functions that return constants and 0x980. Whether the public prototype has unused parameters is not established.

### Unknown function info

- [sceEditMp4Abort, sceEditMp4Edit, sceEditMp4GetStatus, sceEditMp4Initialize, sceEditMp4SetFinishCallback, sceEditMp4Terminate and sceEditMp4Unite](../../../core/libs/prx/libSceEditMp4/Export.cpp) (libSceEditMp4) - answer only the checks firmware 13.42 makes before they use an open handle or Initialize goes on to allocate, read from its disassembly, not measured on a console, and throw through `NotImplemented_nid_no_patch` past them; their declarations carry only the arguments those checks read, and the names are mine. No handle is open here: only a successful Initialize stores one. Each call first returns `0x814B000F` when a byte set by the module's start-up after it creates a mutex is not 1; this implementation answers as if that creation succeeded.
- [sceEditMp4InitializeParam](../../../core/libs/prx/libSceEditMp4/Export.cpp) (libSceEditMp4) - the size of the parameter structure and the meaning of its fields are unknown. It follows firmware 13.42's libSceEditMp4.sprx, read from its disassembly and run natively on x86-64 (not a console): it returns nothing, ignores a null pointer and otherwise clears bytes 0 to 15, stores the 64-bit value 0x3f at offset 16 and the 32-bit value 700 at offset 24, and leaves the bytes from offset 28 unchanged; Firmware 13.42 checks only for null and accesses the structure directly (read from disassembly, not measured on a console), so the console faults on an inaccessible non-null pointer; AnyPS5 skips the write and returns normally instead.
