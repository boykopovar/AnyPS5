# libSceLibcInternal

## add Functional

- [num_put<char>::_Iput, num_put<wchar_t>::_Put and _Rep](../../../core/libs/prx/libSceLibcInternal/NumPutField.cpp) (libSceLibcInternal) follow [microsoft/STL xlocnum](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocnum) and were not compared with the console's `libSceLibcInternal.sprx`; no title was seen calling them. `_Iput` uses the field writer of libc's `num_put<char>::do_put`, so its gaps apply: a stream locale without `ctype<char>` leaves the digits unwidened and one without `numpunct<char>` uses the C-locale facet, where the STL's `use_facet` builds it through `_Getcat`. A null stream or locale and an unreadable digit or source buffer throw `std::invalid_argument`, where the console faults. The wide members write 16-bit elements, as the rest of this library models `wchar_t`.
