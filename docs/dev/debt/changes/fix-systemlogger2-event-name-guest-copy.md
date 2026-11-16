# libSceSystemLogger2

## change [Event record guest pointers]

- [Event record guest pointers](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) (libSceSystemLogger2) - event validation now copies the clock source, record fields, inline names and RNPS token from guest memory rather than dereferencing them directly; `SetEventAsyncEx` also safely reads the embedded entry name and language pointers. `GetEventStatus` copies its event name the same way, and `GetEventStatus` and `SetEventAsyncInternal` write their status output through a checked copy. An unreadable record, name or language, or an unwritable status, returns `0x818C0002` without dereferencing inaccessible host memory, while preserving `errno` and leaving `SetEventAsyncInternal`'s output untouched on a bad record. This is a host-safety fallback: firmware behavior for invalid or partially mapped guest pointers was not measured on a console.
