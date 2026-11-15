# libSceNet

## add Functional

- [sceNetConfigGetIfFlags and sceNetConfigGetDefaultRoute guest outputs](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - write their 32-bit outputs via `GuestCopyOut_nid_no_patch` while preserving `errno` on the write, returning `0x8041130E` for an unwritable `flags` pointer and `fail(NET_EFAULT)` (`0x8041010E`, setting `sceNetErrnoLoc` and `errno` to `EFAULT`) for an unwritable non-null `gateway` pointer when initialized with a route present. These error returns are unmeasured host-safety fallbacks; null-pointer handling and `0x8041130C` without a route are unchanged.
