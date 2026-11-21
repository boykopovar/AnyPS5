# libSceAddressSanitizer

## add Functional

- [ASan runtime](../../../core/libs/prx/libSceAddressSanitizer/src/Asan.cpp) (libSceAddressSanitizer) - 125 `__asan_*` exports (init and version checks, poisoning, `__asan_set_shadow_*`, load, store and report callbacks, globals, `__asan_mem*`) follow compiler-rt 16.0.6 on LLVM's PS shadow (scale 3, offset `1 << 40`), reserved lazily for the 47-bit space. Not measured on a console: the runtime version, the report text, and the 56-byte global descriptor used after `__asan_version_mismatch_check_v6`. Reports have no unwound stack (frame #0 is the caller), heap and stack addresses print as wild pointers, threads print as `T-1`, ODR violations are not detected, and options come from the host `APS5_GUEST_ASAN_OPTIONS`. In a host AddressSanitizer build the host shadow covers this range, so `__asan_init` throws.
