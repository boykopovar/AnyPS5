# libSceSulphaDrv technical debt

### Functional

- [sceSulphaGetDefaultConfig](../../../core/libs/prx/libSceSulphaDrv/Export.cpp) (libSceSulphaDrv) - returning 0x804D0003 from sceSulphaGetDefaultConfig for an unwritable destination buffer is a host-safety fallback; console behaviour for unwritable memory has not been measured.

### Unknown function info

- [libSceSulphaDrv](../../../core/libs/prx/libSceSulphaDrv/Export.cpp) - signatures and behavior remain unknown for `sceSulphaGetClientInfo`, `sceSulphaGetConnectedClientCount`, `sceSulphaGetNeededMemory`, `sceSulphaInit`, `sceSulphaMessage`, `sceSulphaSetBookmark` and `sceSulphaShutdown`. Their no-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch`; they make no ABI claim.
