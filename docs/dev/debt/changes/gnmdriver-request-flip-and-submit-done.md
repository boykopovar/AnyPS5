# libSceGnmDriver

## add Unknown function info

- [sceGnmRequestFlipAndSubmitDone](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the six-argument signature and 0x100-byte, 64-dword flip request follow [shadPS4 at fec7684d](https://github.com/shadps4-emu/shadPS4/blob/fec7684d7eb0a3d5f1c3753cc15e7f582bfd8701/src/core/libraries/gnmdriver/gnmdriver.cpp), but the emitted packets are adapted to AnyPS5's token EOP flip: a WRITE_DATA surface lock, an eight-dword RELEASE_MEM whose token was reserved through `sceVideoOutSubmitEopFlip`, and NOP padding to 64 dwords. A full VideoOut flip queue maps to 0x80d11081, as in shadPS4. Null or unwritable command buffers return 0x80d11000 as a host-safety fallback; console behavior on unmapped memory is unmeasured. The EOP completion label, NOP payload and submit-done boundary are not measured on a console. Closes when: measured on a PS4
