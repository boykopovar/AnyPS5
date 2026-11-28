# libSceLibcInternal

## change [Collation state getters]

- [Collation state getters](../../../core/libs/prx/libSceLibcInternal/CollationState.cpp) (NIDs `bF4eWOM5ouo`, `hzsdjKbFD7g`) are unverified on a console or newer SDKs. SDK 10.00 `libc.prx` at `0x3be00` and `0x3be10` returns distinct writable 128-byte BSS tables at `0x19a0f0` and `0x19a178`; `target/include/xstate.h:22-25` and `target/include/xwchar.h:27-28` establish the sixteen pointer slots and pointer return type. Its `setlocale` at `0x3bdd0` accepts only the C locale without replacing either table. The existing collate facet backend does not consume caller changes to these tables: `_Strcollx` and `_Wcscollx` ignore the state they are passed, and the wide `_LStrcoll` helper remains unimplemented; returning the tables does not provide those collation paths.

## add Functional

- [_Strcollx](../../../core/libs/prx/libSceLibcInternal/Collate.cpp) (libSceLibcInternal, NID `v6rXYSx-WGA`) - not checked against `libSceLibcInternal.sprx` or on a console. The `(const char*, const char*, LibcCollationState*)` signature is inferred from Dinkumware's `strcoll` worker and the state `_Getpcostate` returns. Delegates to `sceLibcInternalCollateDoCompare_nid_no_patch` and ignores the state, so non-C locale collation tables are not supported.
