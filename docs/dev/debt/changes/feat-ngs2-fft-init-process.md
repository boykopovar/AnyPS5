# libSceNgs2

## change [sceNgs2FftQuerySize](../../../core/libs/prx/libSceNgs2/PendingExports.cpp)

- [sceNgs2FftQuerySize](../../../core/libs/prx/libSceNgs2/PendingExports.cpp) (libSceNgs2) - implemented in `libSceNgs2.native/src/System.cpp` from the firmware 11.00 module at `0x430`, whose table is at `0x30b30`: lengths 8..12 select 64, 128, 256, 512 and 1024 samples from the entry at offset 4, bit 1 of the flags halves the count, and the work size is `32 * count + 64`. `-1` for a length outside 8..12 is the module's own return; `-1` for a null output is this implementation's guard, since the module writes through the pointer after validating only the length. No dump imports these NIDs.

## add Functional

- [sceNgs2FftInit and sceNgs2FftProcess](../../../core/libs/prx/libSceNgs2/PendingExports.cpp) (libSceNgs2) - implemented from the firmware 13.42 readings (#21756), unmeasured on a console. `sceNgs2FftInit(work, size, length, flags, handle)` validates length `8..12` and `size >= 32 * count + 64` (halved under `flags & 2`), writes the work header and twiddle tables, and sets `*handle = work` (returning `-1` preserving `errno` on null or inaccessible guest pointers). `sceNgs2FftProcess(handle, in, out, work, unused, mode)` uses a double-precision radix-2 FFT rather than the 13.42 AVX2 kernel (`float` twiddles may differ slightly in precision): with `flags & 2` clear it copies `work` to `out`, and with `flags & 2` set it computes forward/inverse real-to-packed-complex transforms (`0x0`, `0x8`, `0x1`, `0x9`, `0x4`, `0xc`, `0x5`, `0xd`). Mode `0x9` leaves quarters 2 and 3 unscaled per 13.42, and inter-quarter scratch values in modes `0x1`/`0x9` are zeroed.
