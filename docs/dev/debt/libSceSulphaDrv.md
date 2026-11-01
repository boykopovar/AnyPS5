# libSceSulphaDrv technical debt

### Unknown function info

- [libSceSulphaDrv](../../../core/libs/prx/libSceSulphaDrv/Export.cpp) - signatures and behavior remain unknown for `sceSulphaGetClientInfo`, `sceSulphaGetConnectedClientCount`, `sceSulphaGetNeededMemory`, `sceSulphaInit`, `sceSulphaMessage`, `sceSulphaSetBookmark` and `sceSulphaShutdown`. Their no-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch`; they make no ABI claim.
