# libSceVideoArbitration

## change [sceVideoArbitrationAcceptEvent and sceVideoArbitrationEnable]

- [sceVideoArbitrationAcceptEvent and sceVideoArbitrationEnable](../../../core/libs/prx/libSceVideoArbitration/Export.cpp) (libSceVideoArbitration) - from firmware 13.42 `libSceVideoArbitration.sprx` (`0x490`, `0x500`, `0x3c0`), read from disassembly and not run on console: `sceVideoArbitrationEnable(std::uint64_t reserved, void* callback)` and `sceVideoArbitrationAcceptEvent(std::uint32_t event)` match register widths (`rdi`, `rsi`; `edi`), and listener thread `0x3c0` invokes the callback with `int32_t`, though callback return type and event semantics remain unverified.

## change [sceVideoArbitrationInitialize]

- [sceVideoArbitrationInitialize](../../../core/libs/prx/libSceVideoArbitration/Export.cpp) (libSceVideoArbitration) - reads the 0x18-byte parameter block via `GuestCopyIn_nid_no_patch` preserving `errno` and returns `0x819E0002` when null, unreadable, or `size != 0x18` (the console module reads unchecked). The usermode state machine tracks initialization, callback storage, and pending-event checks without creating the `SceVideoArbitration` event flag, validating `+0x8`/`+0x10` fields against pthread attributes, or spawning the listener thread.

## change [The answers without arbitration]

- [The answers without arbitration](../../../core/libs/prx/libSceVideoArbitration/Export.cpp) (libSceVideoArbitration) - in firmware 13.42 `libSceVideoArbitration.sprx` (`0x250`, `0x490`, `0x500`), read from disassembly and not run on a console: `Initialize` returns `0x819E0003` when already initialized; `Enable` returns `0x819E0001` uninitialized, `0x819E0002` if `reserved != 0` or `callback == nullptr`, and `0` otherwise; `AcceptEvent` returns `0x819E0001` uninitialized and `0x819E0003` with no matching pending event.

## add Functional

- [libSceVideoArbitration usermode state](../../../core/libs/prx/libSceVideoArbitration/Export.cpp) (libSceVideoArbitration) - because no `SceVideoArbitration` kernel event flag or listener thread runs, `sceVideoArbitrationInitialize` delivers no arbitration events and `sceVideoArbitrationAcceptEvent` always returns `0x819E0003` after initialization. Closes when: kernel event-flag arbitration producer/listener is wired and tested.
