# libSceLibcInternal

## add Functional

- [time_put<wchar_t>::put](../../../core/libs/prx/libSceLibcInternal/FacetPut.cpp) (libSceLibcInternal, NIDs `Nt6eyVKm+Z4` and `Sc0lXhQG5Ko`) - not checked against the console's `libSceLibcInternal.sprx`, and no title was seen calling them. Unverified: a locale without a `ctype<wchar_t>` narrows the pattern as libc's C-locale `_Wctob`, whatever its name; `narrow` is read at vtable offset 0x70, the microsoft/STL [virtual order](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocale#L2946-L3035); literal elements go through the narrow `Streambuf` layout with 2-byte elements, and a full buffer calls `overflow(int)` with end of file `-1`, the `int` that NID `PjH5dZGfQHQ` (`basic_filebuf<wchar_t>::overflow(int)`) gives. The rest follows the `time_put<wchar_t>` code of the first microsoft/STL commit ([xloctime](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xloctime#L764-L802))
- [time_put<wchar_t>::do_put](../../../core/libs/prx/libSceLibcInternal/FacetPut.cpp) (libSceLibcInternal, NID `Fr7j8dMsy4s`) follows the `time_put<wchar_t>::do_put` of the first microsoft/STL commit ([xloctime](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xloctime#L838-L859)), but formats through this library's C-locale `wcsftime` instead of the facet's `_Tnames`, as libc's `time_put<char>` does through `strftime`. The title-bundled libc from PPSA03671 v01.001.005 was disassembled and differentially tested in [PR #17578](https://github.com/boykopovar/AnyPS5/pull/17578): it sign-extends the specifier and modifier bytes into the wide format and writes nothing when the first 16-element buffer contains only the leading marker. A `wcsftime` failure that returns 0 at every size grows the buffer until allocation fails, as the source does. This was not checked against the console's `libSceLibcInternal.sprx`, and no title was seen calling it.

## remove [_ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPK2tmPKwSB_]

## remove [_ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPK2tmcc]

## remove [_ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewPK2tmcc]
