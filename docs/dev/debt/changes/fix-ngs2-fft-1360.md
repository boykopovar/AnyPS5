# libSceNgs2

## add Functional

- [sceNgs2FftInit and sceNgs2FftProcess 13.60 layout](../../../core/libs/prx/libSceNgs2/PendingExports.cpp) - Verified against firmware 13.60 `libSceNgs2.sprx` (`0x120`, `0x890`, `0x23b0`): `sceNgs2FftInit` stores `secondAddr = tableAddr + count * 24` at `[work + 0x08]`; `sceNgs2FftProcess` real-to-complex forward (`flags & 2`, `mode & 1`) outputs `points` floats in natural `[DC, Nyq, Re1, Im1, ...]` order when `(mode & 4) == 0` and scales all `points` floats by `1 / points` when `(mode & 8) != 0`. Complex-only `(flags & 2) == 0` skips FFT stages in 13.60 and remains stubbed to a zero/work copy.
