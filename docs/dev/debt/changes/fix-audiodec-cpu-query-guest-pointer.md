# libSceAudiodecCpu

## add Functional

- [sceAudiodecCpuQueryMemSize and sceAudiodecCpuInternalQueryMemSize](../../../core/libs/prx/libSceAudiodecCpu/Export.cpp) preserve the documented null-pointer, size and codec-type checks and the 0x190138-byte Blu-ray/DVD LPCM work-memory result while reading only the control parameter pointer and memory descriptor's size through checked guest copies and writing only `ui_memory_size` through a checked guest copy. An inaccessible non-null guest pointer throws `std::runtime_error` rather than crashing the host; the console outcome for such an address is unverified, so no error code is asserted for it.
