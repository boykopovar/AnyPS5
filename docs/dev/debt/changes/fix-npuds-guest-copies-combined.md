# libSceNpUniversalDataSystem

## add Functional

- [Record strings and binary values](../../../core/libs/prx/libSceNpUniversalDataSystem/src/Record.cpp) (libSceNpUniversalDataSystem) - the `IntRecordObjectSet*` keys, the `IntRecord*SetString` values and the `IntCreateRecordData` record type are copied with `CopyGuestString_nid_no_patch` with no length limit, as the direct read had none. The `IntRecord*SetBinary` bytes are copied with `GuestCopyIn_nid_no_patch`. An unreadable key returns `0x80553114`, an unreadable record type `0x8055310D` and an unreadable value `0x80553115`, the codes a null one returns, and nothing is stored. A string too large for host memory returns `0x80553101`. These are host-safety fallbacks; what the console returns for unmapped strings and bytes is unmeasured.
