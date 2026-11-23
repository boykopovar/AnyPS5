# libSceGnmDriver

## change [sceGnmDriverTraceInProgress]
- [sceGnmDriverTraceInProgress](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - returns false: a retail title has no driver trace session in progress, matching the existing Neo provider and shadPS4. A PS5 (firmware 11.60, payload process; a title process may differ) returned 0 from `sceGnmDriverTraceInProgress()` called twice without arguments, as this does; whether a title process has a trace session is not measured, so this stays a retail-only answer, not a host trace state.
