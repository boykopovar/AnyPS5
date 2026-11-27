# libSceLibcInternal

## remove `_Strerror` (NID `4F11tHMpJa0`)

## add Additional PS5 3.20 imports (xdCloudy #9511)

- [_Strerror](../../../core/libs/prx/libSceLibcInternal/InventoryExports2.cpp) (libSceLibcInternal, NID `4F11tHMpJa0`) - no module was read and no console measured. The prototype `char* _Strerror(int, char*)` and the split between fixed messages, returned without touching the buffer, and numbered ones written into it (a static buffer when it is null) follow Dinkumware's C library as published in P. J. Plauger's The Standard C Library. The texts are libc's `strerror` texts; the 22-byte `error_code=0x%08x` text written for other codes is inferred from `strerror_r`'s 23-byte minimum, not measured. The null-buffer fallback is per thread, where Dinkumware's is shared
