# libSceSystemLogger2

## remove [validateEvent, validateEventEx and validateEventEx2]

## change [Event record guest pointers]

- [Event record guest pointers](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) (libSceSystemLogger2) - event validation now copies the clock source, record fields, inline names and RNPS token from guest memory rather than dereferencing them directly; `SetEventAsyncEx` also safely reads the embedded entry name and language pointers. An unreadable record, name or language returns `0x818C0002` without dereferencing inaccessible host memory, while preserving `errno` and leaving `SetEventAsyncInternal`'s output untouched; this includes a record whose inline name runs into an unmapped page, while a name that ends just before it is accepted. This is a host-safety fallback: firmware behavior for invalid or partially mapped guest pointers was not measured on a console.
