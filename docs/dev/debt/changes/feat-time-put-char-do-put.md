# libSceLibcInternal

## add Functional

- [time_put<char>::do_put](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal, NID `+i81FtUCarA`) is libc's `time_put<char>` `do_put`, the one in the facet that libc's `time_put<char>::_Getcat` builds. It follows the `time_put` `do_put` of the first microsoft/STL commit ([xloctime](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xloctime#L713-L735)), but formats through this library's `strftime` instead of the facet's `_Tnames`, so the libc `time_put` and `strftime` gaps apply. It was not checked against the console's `libSceLibcInternal.sprx`, and no title was seen calling it.

## remove [_ZNKSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecPK2tmcc]
