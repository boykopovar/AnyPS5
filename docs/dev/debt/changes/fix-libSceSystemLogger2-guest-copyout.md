# libSceSystemLogger2

## add

- Output pointer validation (libSceSystemLogger2) - `sceSystemLogger2GetTimestamp`, `sceSystemLogger2GetEventStatus`, and `sceSystemLogger2SetEventAsyncInternal` now safely validate and copy out results using `GuestCopyOut_nid_no_patch` while preserving `errno`. When output pointers target unmapped or unwritable memory, or when `eventName` in `sceSystemLogger2GetEventStatus` is unmapped or exceeds bounds, `0x818C0002` (`INVALID_ARGUMENT`) is returned. This is a host-safety fallback; console hardware behavior on invalid destination addresses was not measured.
  Labels: code, console
