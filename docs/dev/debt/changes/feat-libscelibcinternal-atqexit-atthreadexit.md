# libSceLibcInternal

## add Functional

- [_Atqexit and _Atthreadexit](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - console behavior and registration capacity are unverified; no title was observed calling these exports. `_Atqexit` forwards to libc's `at_quick_exit` to register quick-exit handlers in LIFO order upon `quick_exit()`. `_Atthreadexit` uses `HostThreadLocal` and `sceKernelSetThreadDtors` to register thread-local exit callbacks in LIFO order upon thread termination or `_sceLibcInternalThreadDtors`, executing after static TLS thread destructors. Passing a null callback returns 0 immediately without registration. Closes when: verified against console libSceLibcInternal behavior.
