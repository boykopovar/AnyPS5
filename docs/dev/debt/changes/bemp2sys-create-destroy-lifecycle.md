# libSceBemp2sys

## change [libSceBemp2sys pending exports]

- [libSceBemp2sys pending exports](../../../core/libs/prx/libSceBemp2sys/Export.cpp) - 3 named release calls (`bemp2sys_packetizer_release`, `bemp2sys_pesparser_release`, and `bemp2sys_tsfilter_release`) use verified `void(void*)` declarations and safely no-op on null, while throwing through `NotImplemented_nid_no_patch` on non-null handles because their internal callees (`vtable+0x18`, `0x9cf0`, `0x10360`) are unexamined.

## change [The parser objects' argument checks]

- [The parser objects' argument checks](../../../core/libs/prx/libSceBemp2sys/Export.cpp) (libSceBemp2sys) - the create, destroy, and `bemp2sys_pesparser_init` calls are implemented, while the remaining 11 named calls plus `B1nK+wb5rME` keep their verified argument and null-field checks before throwing through `NotImplemented_nid_no_patch` on unexamined body paths. Only the 32-bit return values and verified parameter widths are claimed.

## add Functional

- [bemp2sys create, destroy, and verified init/release lifecycle](../../../core/libs/prx/libSceBemp2sys/Export.cpp) (libSceBemp2sys) - `bemp2sys_packetizer_create` validates the out pointer (`0x81001001` on null/unwritable) and type mask `{0x10000001, 0x10000002, 0x10000004, 0x10000008, 0x10000010, 0x10000020}` (`0x81004001` on invalid type or allocation failure, writing null); `bemp2sys_{pesparser,psdecoder,tsdecoder,tsfilter}_create` allocate handles via `GuestCopyOut_nid_no_patch`; all 5 destroy calls safely no-op on null and free handles; `bemp2sys_pesparser_init` sets parser state (`0x5c00`/`0x9ce0`); `bemp2sys_tsfilter_init` returns `0x81004001` on null first parameter word (`0x10530`); the 3 non-decoder release calls no-op on null (`0x5b30`/`0x5c40`/`0x7020`). Read from firmware 13.42 disassembly.
