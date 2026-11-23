# libSceRtc
## change [sceRtcCheckValid]
- [sceRtcCheckValid](../../../core/libs/prx/libSceRtc/Export.cpp) (libSceRtc) - returning SCE_RTC_ERROR_INVALID_POINTER (0x80B50002) for unmapped or unreadable parameter pointers is a host-safety fallback; on a PS5 (firmware 11.60, payload process; a title process may differ) a null pointer returned `0x80B50002` and a pointer to unmapped memory (`0x1`) raised SIGSEGV, so there the call faults where this library returns the error code. The pointer cases after the first fault were not run.
