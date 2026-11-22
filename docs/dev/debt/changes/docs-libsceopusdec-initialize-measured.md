# libSceOpusDec

## change [sceOpusDecInitialize]
- [sceOpusDecInitialize](../../../core/libs/prx/libSceOpusDec/include/OpusDecoder.hpp) (libSceOpusDec, shared with `sceOpusCeltDecInitialize`) - returning `0x8174000d` (`0x8140000d` for CELT) when `context` is unwritable via `GuestCopyOut_nid_no_patch` is a host-safety fallback. A PS5 (firmware 11.60, payload process; a title process may differ) returned `0x8174000d` for `sceOpusDecInitialize(NULL)`, as this does, and faulted with SIGSEGV on a call with all five argument registers set to the unmapped address 0x10; which of the five was read is not isolated, and the unwritable-`context` case alone was not called, so what the console does there stays unmeasured.
