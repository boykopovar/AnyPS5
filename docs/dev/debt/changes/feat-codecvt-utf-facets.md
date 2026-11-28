# libc

## add Functional

- [codecvt<char16_t,char,_Mbstatet> and codecvt<char32_t,char,_Mbstatet> vtables](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc, NIDs `jlNI3SSF41o` and `9H2BStEAAMg`, ids `xM5re58mxj8` and `u2MAta5SS84`) are thirteen `uintptr_t` slots each: offset-to-top, typeinfo, the two destructors, `_Incref`/`_Decref`, `codecvt_base`'s `do_always_noconv`, `do_max_length` and `do_encoding`, then `do_in`, `do_out`, `do_unshift` and `do_length`, in the declaration order of [microsoft/STL `xlocale`](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocale#L691-L704) (lines 965-1188 and 1264-1423). That is the shape of the console's `codecvt<char>` vtable (`st_size` 104); these two sizes were not measured. Offset-to-top and typeinfo stay zero, and the ids are 8-byte zeros like the other facet ids.
- [codecvt<char16_t,char,_Mbstatet> and codecvt<char32_t,char,_Mbstatet> do_in and do_out](../../../core/libs/prx/libc/src/CodecvtUtf.cpp) (libc, NIDs `+uPwCGfmJHs`, `0FKwlv9iH1c`, `PJ2UDX9Tvwg` and `eoW60zcLT8Q`) were not compared with the console's modules or measured. They follow microsoft/STL 219514876e `xlocale` lines 965-1150 and 1264-1394, which Dinkumware V6.00 (crt.vc12 f1d47126bf `xlocale` lines 1081-1255 and 1405-1525) matches. `_Maxcode` is read as the 8-byte `unsigned long` at 0x10 and `_Mode` as the 4-byte enum at 0x18. The retry after a byte order mark calls the vtable's `do_in` slot at 0x38; virtual dispatch on the console is unverified.
- [codecvt<char16_t> and codecvt<char32_t> metadata, unshift and destructors](../../../core/libs/prx/libc/src/CodecvtUtf.cpp) (libc) - `do_unshift`, `do_max_length` and the `char32_t` `do_encoding` moved unchanged from libSceLibcInternal, whose entry cited SDK 10.00 `xlocale` and the SDK's `libc.prx` for them, and match crt.vc12 f1d47126bf `xlocale` lines 1257-1265, 1304-1309, 1527-1532 and 1571-1581; an unreadable facet mode, shift state or output pointer throws. `do_always_noconv` returns false and the `char16_t` `do_encoding` 0 (lines 1299-1302, 1311-1314 and 1566-1569). The destructors are empty (lines 1073-1075 and 1397-1399) and the deleting ones then call `operator delete`; a null facet throws `std::invalid_argument`. Console execution is unverified.
- [codecvt<char16_t,char,_Mbstatet> and codecvt<char32_t,char,_Mbstatet> do_length](../../../core/libs/prx/libc/src/CodecvtUtf.cpp) (libc, NIDs `lynApfiP6Lw` and `m6rjfL4aMcA`) throw because the two Dinkumware versions differ. V6.00 (crt.vc12 f1d47126bf `xlocale` lines 1267-1297 and 1534-1564) counts the characters that the virtual `do_in` converts from a copy of the state. microsoft/STL 219514876e (`xlocale` lines 850-891) returns the bytes consumed and advances the caller's state. No source here shows which one the console's libc has.

# libSceLibcInternal

## change [UTF codecvt metadata and unshift]

- [UTF codecvt metadata and unshift](../../../core/libs/prx/libSceLibcInternal/CodecvtUtfMetadata.cpp) (libSceLibcInternal) - the five wrappers forward to libc's `codecvt<char16_t>` and `codecvt<char32_t>` bodies in [CodecvtUtf.cpp](../../../core/libs/prx/libc/src/CodecvtUtf.cpp), where their gaps are recorded. They followed SDK 10.00 `target/include/xlocale:1446-1455,1493-1499,1717-1723,1761-1772` and `target/sce_module/libc.prx`: char32 unshift at `0xb31b0`, encoding at `0xb2e40`, maximum length at `0xb2e30`; char16 unshift at `0xb2cf0`, maximum length at `0xb27d0`. Names come from `libc.prx.sym`. Firmware fault behavior is unmeasured.

## add Functional

- [codecvt<char16_t> and codecvt<char32_t> do_in, do_out and deleting destructors](../../../core/libs/prx/libSceLibcInternal/Err.cpp) (libSceLibcInternal, NIDs `+uPwCGfmJHs`, `0FKwlv9iH1c`, `PJ2UDX9Tvwg`, `eoW60zcLT8Q`, and `zYHryd8vd0w` and `vwMx2NhWdLw` in [InventoryExports.cpp](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp)) forward to libc's bodies in [CodecvtUtf.cpp](../../../core/libs/prx/libc/src/CodecvtUtf.cpp), where their gaps are recorded.

## remove [_ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6_]

## remove [_ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6_]

## remove [_ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6_]

## remove [_ZNKSt7codecvtIDsc9_MbstatetE6do_outERS0_PKDsS4_RS4_PcS6_RS6_]
