# libSceIcu

## add Functional

- [Tracing](../../../core/libs/prx/libSceIcu/Trace.cpp) has no console ABI or behavior verification. The ten `utrace_*` exports take the signatures of ICU 68.2's [utrace.h](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/unicode/utrace.h) and follow [utrace.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utrace.cpp), whose behavior 78.1 keeps; `va_list` arguments are the guest's. `u_cleanup` resets the functions and turns tracing off as ICU's does, but no export emits trace events. An unknown `utrace_exit` return type, a null `utrace_getFunctions` pointer, format or buffer with a capacity, and a null `va_list` that a conversion reads throw where ICU aborts or dereferences them. Closes when: compared with the console.
