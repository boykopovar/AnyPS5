# libkernel

## change [sceKernelIsAddressSanitizerEnabled]

- [sceKernelIsAddressSanitizerEnabled](../../../core/libs/prx/libkernel/System/src/Sanitizer.cpp) - returns 0 for every call, as a payload process on a PS5 (firmware 11.60) did in ten calls, five in the main thread and five in a second thread (run `2026-11-14T035511Z_R1_libkernel_sanitizer_flag`). The host always reports disabled because guest sanitizer activation is not modeled; what the query returns while a sanitizer is active, and in a title process, is not measured. PPSA17641, PPSA85300 (VLC), PPSA98766, PPSA99203, PPSA99505 and PPSA99666 (DOOM) import it (NID `jh+8XiK4LeE`, one reference each), and the console exports that NID from `libkernel`, `libkernel_sys` and `libkernel_web`.
