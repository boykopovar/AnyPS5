# libkernel_sys

## change [scePthreadBarrier*, scePthreadBarrierattr*, scePthreadGetcpuclockid, scePthreadGetconcurrency and sceKernelMsync]

- [scePthreadBarrier*, scePthreadBarrierattr*, scePthreadGetcpuclockid, scePthreadGetconcurrency and sceKernelMsync](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) wrap POSIX errors as `0x80020000 | errno`; signatures follow the Orbital type database. Barrier names are firmware-dependent: 11.60 refused 40 characters, while a PS5 Pro payload on 13.60 accepted 31, 32, 39 and 40; count 0 returns `EINVAL`; titles were not measured. `scePthreadGetcpuclockid` returns plain `CLOCK_THREAD_CPUTIME_ID` (14) for self and another live thread on 13.60, as here; POSIX `pthread_getcpuclockid` keeps bit-31 per-thread ids for `clock_gettime`. `scePthreadGetconcurrency` returns 0 at level 0 and `0x80020000 | level` otherwise (11.60; level 0 re-confirmed on 13.60); titles may differ.
