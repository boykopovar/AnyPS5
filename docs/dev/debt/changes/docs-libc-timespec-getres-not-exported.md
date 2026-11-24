# libc

## change [timespec_getres]

- [timespec_getres](../../../core/libs/prx/libc/src/Time.cpp) (libc, NID `ZvRHHCfA4HA`) has unverified actual console clock behavior and SDK-version coverage; the console's `libSceLibcInternal.sprx` does not export it (`dlsym` returned null in a PS5 payload process, firmware 11.60), and a title's own `libc.prx` is unmeasured. It follows the bundled PPSA03671 v01.001.005 wrapper: only base 1 forwards to `sceKernelClockGetres` with clock ID 0, returning 1 exactly when that call returns 0. A missing kernel provider throws. Null results, output validation, errno and resolution follow the loaded kernel provider; no separate host-clock policy is added. Delegation is checked by CPU-only execution of the original wrapper with a controlled imported callee.
