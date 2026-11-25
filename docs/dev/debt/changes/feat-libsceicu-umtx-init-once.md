# libSceIcu

## add Functional

- [umtx_initImplPreInit and umtx_initImplPostInit](../../../core/libs/prx/libSceIcu/Mutex.cpp) (libSceIcu) - not compared with the console. They follow [umutex.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/umutex.cpp#L144-L176) and gave the results of ICU 68.2 on the same calls, with one host mutex and condition variable for every `UInitOnce`, and read and write only its `fState` atomically in guest memory (the ICU 68.2 layout of `umutex.h`). Unlike ICU, `u_cleanup` does not reset that mutex. A null `UInitOnce` throws `std::invalid_argument`; ICU 68.2 crashes. `umtx_initImplPostInit` called before the first `umtx_initImplPreInit` or `UMutex` lock of the process sets the state to 2; ICU 68.2 then locks a mutex it has not created yet (`std::system_error` with libstdc++).
