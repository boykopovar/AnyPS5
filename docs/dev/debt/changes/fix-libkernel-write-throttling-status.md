# libkernel

## change [sceKernelWriteThrottlingStatus]

- [sceKernelWriteThrottlingStatus](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) returns `ESRCH` (`0x80020003`) for a valid 32-byte status buffer and leaves all four qwords untouched, as a payload process on a PS5 (firmware 11.60) did. A null status pointer returns `EINVAL` (`0x80020016`), also as measured there. An unwritable non-null buffer returns `EFAULT` (`0x8002000e`) through a non-mutating guest writeability check as a host-safety fallback; console fault behaviour and title-process behaviour are unmeasured. PPSA17221 reads only the first qword on success, but on failure assumes `0x4B000001` bytes, which selects the same highest write-budget tier as the previous host-defined `UINT32_MAX` success. What the hardware status value measures and the meanings of the other qwords remain unknown.
