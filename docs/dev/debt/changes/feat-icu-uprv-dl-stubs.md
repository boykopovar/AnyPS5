# libSceIcu

## add Functional

- [Dynamic loading stubs](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) (libSceIcu) - `uprv_dl_open`, `uprv_dl_close` and `uprv_dlsym_func` assume ICU's disabled dynamic library loading path (`U_ENABLE_DYLOAD=0`), reporting `U_UNSUPPORTED_ERROR` and validating guest memory status pointers; console firmware evidence of whether `libSceIcu.sprx` imports `dlopen`/`dlsym` or uses the disabled fallback remains unverified. Closes when: compared with the console.
