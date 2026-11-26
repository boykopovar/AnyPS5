# libkernel

## change [mlock, munlock, mlockall, munlockall]

- [mlock, munlock, mlockall, munlockall](../../../core/libs/prx/libkernel/DirectMemory/PosixMemory.cpp) (libkernel) - `mlock` and `munlock` forward to the `sceKernelMlock` and `sceKernelMunlock` host-page paths, mapping their errors to `EINVAL` or `ENOMEM`: a payload process on a PS5 Pro (firmware 13.60) locked a mapped page with rc 0, so the earlier jail `EPERM` model read from FreeBSD 11.4 `priv_check` (#11216) does not hold on the console; failure codes mirror the host and were not each measured. `mlockall` and `munlockall` still return `EPERM` unchecked, unmeasured on a console; XPSemu (PPSA97358) imports both, which QEMU calls only with `-overcommit mem-lock=on`. `sceKernelMlock`, `sceKernelMunlock` and `sceKernelMunlockall` lock and unlock host pages; `sceKernelMlockall` is a stub in libkernel_sys that returns 0
