# libc

## add Functional

- [codecvt<char16_t,char,_Mbstatet> and codecvt<char32_t,char,_Mbstatet> do_length implemented](../../../core/libs/prx/libc/src/CodecvtUtf.cpp) (libc, NIDs `lynApfiP6Lw` and `m6rjfL4aMcA`) - console modules and newer SDKs were not measured; supersedes the throwing note in `feat-codecvt-utf-facets.md`. Matches C++ [locale.codecvt.virtuals] and Microsoft STL [_Codecvt_do_length](https://github.com/microsoft/STL/blob/165a67afdde92bf3e5248be35b6f9240b3d1efb5/stl/inc/xlocale#L766-L806) ([crt.vc12 V6.00](https://github.com/ganboing/crt.vc12/blob/f1d47126bf13d1c9dccaa6c53b72217fd0474968/src/xlocale#L1267-L1297), [UTF-32](https://github.com/ganboing/crt.vc12/blob/f1d47126bf13d1c9dccaa6c53b72217fd0474968/src/xlocale#L1534-L1564)): calls the facet's UTF-16/UTF-32 `do_in` with a one-unit stack buffer, updates `state`, stops on error/partial, handles `noconv`, and clamps to `INT_MAX`.

# libSceLibcInternal

## remove [_ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m]

## remove [_ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m]

## add Functional

- [codecvt<char16_t> and codecvt<char32_t> do_length](../../../core/libs/prx/libSceLibcInternal/Err.cpp) (libSceLibcInternal, NIDs `lynApfiP6Lw` and `m6rjfL4aMcA`) forward to libc's `CodecvtUtf16Length_nid_no_patch` and `CodecvtUtf32Length_nid_no_patch` in [CodecvtUtf.cpp](../../../core/libs/prx/libc/src/CodecvtUtf.cpp), where their source contract and console gaps are recorded.
