# libSceLibcInternal

## add Functional

- [time_put<wchar_t>::put](../../../core/libs/prx/libSceLibcInternal/FacetPut.cpp) (libSceLibcInternal, NIDs `Nt6eyVKm+Z4` and `Sc0lXhQG5Ko`) - not checked against the console's `libSceLibcInternal.sprx`, and no title was seen calling them. Unverified: a locale without a `ctype<wchar_t>` narrows the pattern as libc's C-locale `_Wctob`, whatever its name; `narrow` is read at vtable offset 0x70, the microsoft/STL [virtual order](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocale#L2946-L3035); literal elements go through the narrow `Streambuf` layout with 2-byte elements, and a full buffer calls `overflow(int)` with end of file `-1`, the `int` that NID `PjH5dZGfQHQ` (`basic_filebuf<wchar_t>::overflow(int)`) gives. The rest follows the `time_put<wchar_t>` code of the first microsoft/STL commit ([xloctime](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xloctime#L764-L802))
- [time_put<wchar_t>::do_put](../../../core/libs/prx/libSceLibcInternal/FacetPut.cpp) (libSceLibcInternal, NID `Fr7j8dMsy4s`) follows the `time_put<wchar_t>::do_put` of the first microsoft/STL commit ([xloctime](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xloctime#L838-L859)), but formats through this library's C-locale `wcsftime` instead of the facet's `_Tnames`, as libc's `time_put<char>` does through `strftime`. It throws where no source settles the result: for a specifier or modifier above 0x7f, as the signedness of the console's `char` is unknown, and for a conversion that writes nothing into the first 16-element buffer, because the measured `wcsftime` leaves out a conversion that does not fit, while the source retries only on 0 and the console's `_Wcsftime` (NID `RGc3P3UScjY`) was not measured. A `wcsftime` failure that returns 0 at every size grows the buffer until allocation fails, as the source does. No title was seen calling it.

## remove [_ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPK2tmPKwSB_]

## remove [_ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPK2tmcc]

## remove [_ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewPK2tmcc]
