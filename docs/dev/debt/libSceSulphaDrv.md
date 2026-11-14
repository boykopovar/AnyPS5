# libSceSulphaDrv technical debt

### Functional

- [sceSulphaGetClientInfo, sceSulphaGetConnectedClientCount, sceSulphaMessage, sceSulphaSetBookmark and sceSulphaShutdown](../../../core/libs/prx/libSceSulphaDrv/Export.cpp) (libSceSulphaDrv) - only the answers firmware 13.42 gives while `sceSulphaInit` has not succeeded are implemented, from its disassembly, not measured on a console: `0x804D0003` for a client index above 1 or a null info pointer (GetClientInfo), a message type above 2 or a null message (Message) and a null bookmark (SetBookmark), otherwise `0x804D0002`. `sceSulphaInit` still throws past its checks, so the module is never initialized here, and the paths of an initialized module are not implemented.
- [sceSulphaGetNeededMemory and sceSulphaInit](../../../core/libs/prx/libSceSulphaDrv/Export.cpp) (libSceSulphaDrv) - only the configuration checks firmware 13.42 makes before any call are implemented, from its disassembly, not measured on a console: each returns `0x804D0003` for a null configuration (or a null result pointer, GetNeededMemory), a value above 3 at offset 0x10, a size at offset 0 other than 0x18 and 0x230, a value at offset 8 below 0x400 or equal to 0xFFFFFFFF, or a value above 0xFFFF at offset 0x14, read in that order. Past these checks, and when a field cannot be read, both throw. `sceSulphaInit` answers `0x804D0001` while another call holds the module's state; concurrent calls are not modelled.
- [sceSulphaGetDefaultConfig](../../../core/libs/prx/libSceSulphaDrv/Export.cpp) (libSceSulphaDrv) - returning 0x804D0003 from sceSulphaGetDefaultConfig for an unwritable destination buffer is a host-safety fallback; console behaviour for unwritable memory has not been measured.

### Unknown function info

- [libSceSulphaDrv](../../../core/libs/prx/libSceSulphaDrv/Export.cpp) - `sceSulphaGetClientInfo`, `sceSulphaGetNeededMemory`, `sceSulphaInit`, `sceSulphaMessage` and `sceSulphaSetBookmark` declare only the arguments the checks under Functional read, with widths from the module's compares and names that are not from a header; their other parameters, and those of `sceSulphaGetConnectedClientCount` and `sceSulphaShutdown`, are unknown.
