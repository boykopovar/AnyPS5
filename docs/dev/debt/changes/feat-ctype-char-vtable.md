# libc

## change [ctype<char> do_widen and do_narrow]

- [ctype<char> do_widen and do_narrow](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc) follow the Dinkumware `ctype<char>` in [microsoft/STL `xlocale`](https://github.com/microsoft/STL/blob/e1ed302683e5e822f663fbb0f44cee7e9ddc6af0/stl/inc/xlocale#L2818-L2838): the single-character forms return the character and ignore the facet and the narrow default, and the range forms copy `[first, last)` to the destination and return `last`. An empty range returns `last` without touching the destination, so a null destination is accepted there. `GuestLocale::CtypeVtable` places `narrow` and `narrowRange` after `widenRange` at 0x50 and 0x58, in declaration order as the Itanium ABI lays out virtual functions; the console `libc.prx` bodies were not read.

## change [ctype<char> vtable]

- [ctype<char> vtable](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc, NID `GKWcAz6-G7k`) is fourteen `uintptr_t` slots matching `st_size` 112 on the title `libc.prx` from PPSA16833 (offset-to-top, typeinfo, two destructors, `_Incref`/`_Decref`, and eight `do_*` virtuals). Slots 2 through 13 hold libc's `ctype<char>` destructors, the reference counting of the other facets, and `do_tolower`, `do_toupper`, `do_widen` and `do_narrow` with their range forms, in the declaration order of Dinkumware's `ctype<char>` ([microsoft/STL `xlocale`](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocale#L2777-L2843), [crt.vc12 `xlocale`](https://github.com/ganboing/crt.vc12/blob/f1d47126bf13d1c9dccaa6c53b72217fd0474968/src/xlocale#L2492-L2566)). Offset-to-top and typeinfo remain zero, unlike the console's relocated entries.

## add Functional

- [ctype<char> do_tolower and do_toupper](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc, NIDs `WXOoCK+kqwY`, `2w+4Mo2DPro`, `mnq3tbhCs34`, `7glioH0t9HM`) - Later-firmware parity remains unverified. SDK 10.00 `libc.prx` at `0x84080`/`0x840c0` uses lower/upper tables at facet offsets `0x18`/`0x20`, unsigned byte indices and each short entry's low byte. Range loops `0x840a0`/`0x840e0` reload the table per byte and return the end; empty ranges skip facet access (`0x84093`/`0x840d3`). Signatures/layout: SDK 10.00 `target/include/xlocale:2461-2486`, `target/include/xlocinfo:51-57`. Unusable memory, null tables and inverted ranges throw `std::invalid_argument` as host-safety policy, not SDK error returns; prefix writes survive later faults. No console measurement is available.
- [ctype<char> destructors](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc, NIDs `tPsGA6EzNKA`, `p8-44cVeO84`) follow `~ctype` and `_Tidy` of Dinkumware's `ctype<char>` ([microsoft/STL `xlocale`](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocale#L2777-L2793), [crt.vc12 `xlocale`](https://github.com/ganboing/crt.vc12/blob/f1d47126bf13d1c9dccaa6c53b72217fd0474968/src/xlocale#L2492-L2510)): the classification table goes to `free` when `deleteTable` is positive and to `delete[]` when it is negative, and the deleting destructor then releases the facet with `operator delete`. Both sources also free a Windows locale name, which `GuestLocale::CtypeFacet` has no field for. A null facet throws `std::invalid_argument` where the console faults. The console's bodies were not read.

# libSceLibcInternal

## remove [ctype character case conversion]

## remove `_ZNSt5ctypeIcED0Ev`

## remove `_ZNSt5ctypeIcED1Ev`

## add Functional

- [ctype<char> do_tolower, do_toupper and destructors](../../../core/libs/prx/libSceLibcInternal/CtypeCase.cpp) (libSceLibcInternal, NIDs `WXOoCK+kqwY`, `2w+4Mo2DPro`, `mnq3tbhCs34`, `7glioH0t9HM`, `tPsGA6EzNKA`, `p8-44cVeO84`) call the `_nid_no_patch` bodies of the libc exports of the same names, not the exports, so that after NID patching the module does not import the NIDs it exports (the 6c5bee0282 pattern that `patched_self_imports` checks). The [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c) also lists them for this module. The console's `libSceLibcInternal.sprx` was not read.
