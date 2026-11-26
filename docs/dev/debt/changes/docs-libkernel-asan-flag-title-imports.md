# libkernel

## change [sceKernelIsAddressSanitizerEnabled]

- [sceKernelIsAddressSanitizerEnabled](../../../core/libs/prx/libkernel/System/src/Sanitizer.cpp) - returns 0 for every call. The host always reports disabled because guest sanitizer activation is not modeled; what the native query reads is not measured. PPSA17641, PPSA85300 (VLC), PPSA98766, PPSA99203, PPSA99505 and PPSA99666 (DOOM) import it (NID `jh+8XiK4LeE`, one reference each), and the console exports that NID from `libkernel`, `libkernel_sys` and `libkernel_web`.
