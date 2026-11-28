# libSceAgcDriver

## add Functional

- [GPU debug state keeps the mask and the value without the console's two notifications](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/GpuDebug.cpp) (libSceAgcDriver) - `sceAgdSetApplicationGpuDebug` rejects bits the system mask does not hold with `0x8a6c0047` and stores the rest in the module's `0x22908 + 0x234`, as the 73-byte export at `rva 0x7990` does. After storing it calls `0x7a60(value & 1)` and `0x5340(bits 1..8)`, which are not reproduced; what those two do to the host is unmeasured. The system mask at `0x22908 + 0x230` is only ever read in the module's own code, and its static initial value is zero, so every non-zero request currently returns `0x8a6c0047`; the writer of that mask is not located. Closes when: a title that sets a non-zero GPU debug value shows what the console's notifications change, or the mask's writer is identified.
