# libSceLibcInternal

## add Functional

- [time_put<wchar_t>::_Getcat](../../../core/libs/prx/libSceLibcInternal/FacetPut.cpp) (libSceLibcInternal, NID `bF2uVCqVhBY`) runs libc's export of the same name, which follows the first microsoft/STL commit ([xloctime](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xloctime#L816-L823)): it returns `_X_TIME`, FreeBSD 11.4's `LC_TIME` 5, and builds a facet only when `*_Ppf` is null, with no references and the `do_put` of NID `Fr7j8dMsy4s`. The source passes `_Locinfo(_Ploc->c_str())`, which only calls `setlocale` ([locale0.cpp](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/src/locale0.cpp#L200-L210)), and libc's `setlocale` accepts only C, so every locale gets the C-locale facet. A null `_Ploc`, which the source dereferences, throws. The facet is the 16-byte facet header alone, as its virtuals read nothing past it. The console module was not read, and no title was seen calling it.

## remove `_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5_`

# libc

## add Functional

- [time_put<wchar_t>::_Getcat and time_put<wchar_t>::id](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc, NIDs `bF2uVCqVhBY` and `JFiji2DpvXQ`) are libc exports in the [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/stubs/by_library/libc.txt#L973) (lines 973 and 1794). `_Getcat` is the function libSceLibcInternal's export of the same NID runs (see its debt entry). The id is an 8-byte zero like the other facet ids, as microsoft/STL's `locale::id` holds one `size_t` that starts at 0 ([xlocale](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocale#L98-L118), [xloctime](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xloctime#L871-L872)). The console's `libc.prx` was not read.
