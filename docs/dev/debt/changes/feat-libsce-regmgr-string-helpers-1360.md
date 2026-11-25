# libSceRegMgr

## change [sceRegMgrMemset]

- [sceRegMgrMemset](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c) resolves the NID (`7WBGJ8aph1M`) to the name only; the signature was measured on a PS5 Pro payload process (13.60): it is the libc `memset(void*, int, size_t)` returning the destination pointer with raw fill semantics, and a null destination returns `(void*)1` without writing (four null arguments answer 1). Implemented as that mapping; unwritable non-null destinations are not guarded - like the console, which ended the session when asked to fill far past a buffer.

## change [sceRegMgrStrncpy]

- [sceRegMgrStrncpy](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c) resolves the NID (`BnSHkcDlM+8`) to the name only; measured on a PS5 Pro payload process (13.60): it is the libc `strncpy(char*, const char*, size_t)` returning the destination, copying exactly `size` bytes with no padding or terminator beyond what `strncpy` itself does, and a null destination or source returns `(char*)1` without writing. Implemented as that mapping.

## change [sceRegMgrGetIntInitVal]

- [sceRegMgrGetIntInitVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - signature `(std::uint32_t optionId, std::int32_t* outInt)` as declared in `include/Api.hpp`; the body throws `NotImplemented_nid_no_patch`, and the forwarding is described under Initial values. Measured on a PS5 Pro payload process (13.60): null arguments answer `0x800D0202` and keys 1 to 3 with a writable output answer `0x800D0203` without writing - unknown-key handling before any registry lookup is reachable, but the factory-default values still need a defaults source.
