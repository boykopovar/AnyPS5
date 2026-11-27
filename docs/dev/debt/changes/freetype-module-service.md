# libSceFreeTypeOt

## add Functional

- [ft_module_get_service](../../../core/libs/prx/libSceFreeTypeOt/src/ModuleService.cpp) (libSceFreeTypeOt, NID `uSNqxrARpEQ`) - console prototype, LP64 record offsets and version equivalence are unverified. Behavior follows FreeType 2.4.12 [ftobjs.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftobjs.c), with guest module/library prefixes from that version's `ftobjs.h` and guest-ABI requesters. Native libSceFreeType adapter handles are not compatible. Inaccessible records, absent classes or fallback libraries, null module entries and counts above the source's 32-slot limit throw instead of reproducing native faults or out-of-bounds accesses. Service IDs, including null, go unchanged to requesters, which decide their validity.
