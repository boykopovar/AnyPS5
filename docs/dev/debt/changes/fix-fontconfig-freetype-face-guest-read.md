# libfontconfig

## change [FreeType character lookup]

- [FreeType character lookup](../../../core/libs/prx/libfontconfig/Export.cpp): `FcFreeTypeCharIndex` follows the pinned Fontconfig `fcfreetype.c` search order: current Unicode or Microsoft Symbol charmap first, then the other; symbol characters below U+0100 also try U+F000 offset. It calls libSceFreeType's guest face exports, so only faces created by that adapter are supported. The console's Fontconfig version, call signature and behavior are unmeasured. Returning 0 for an unreadable guest face or charmap pointer through `GuestCopyIn_nid_no_patch` is a host-safety fallback.
