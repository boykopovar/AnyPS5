# libSceLibcInternal

## add Functional

- [codecvt<char16_t,char,_Mbstatet> and codecvt<char32_t,char,_Mbstatet> do_in and do_out](../../../core/libs/prx/libSceLibcInternal/CodecvtUtfConversion.cpp) (libSceLibcInternal, NIDs `+uPwCGfmJHs`, `0FKwlv9iH1c`, `PJ2UDX9Tvwg` and `eoW60zcLT8Q`) were not compared with the console's modules or measured, and no title was seen calling them. They follow microsoft/STL 219514876e `xlocale` lines 965-1150 and 1264-1394, which Dinkumware V6.00 (crt.vc12 f1d47126bf `xlocale` lines 1081-1255 and 1405-1525) matches. `_Maxcode` is read as the 8-byte `unsigned long` at 0x10 and `_Mode` as the 4-byte enum at 0x18. The retry after a byte order mark calls the vtable's `do_in` slot at 0x38, as the char codecvt does; virtual dispatch on the console is unverified.

## remove [_ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6_]

## remove [_ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6_]

## remove [_ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6_]

## remove [_ZNKSt7codecvtIDsc9_MbstatetE6do_outERS0_PKDsS4_RS4_PcS6_RS6_]

## change [_ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m]

- [_ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m](../../../core/libs/prx/libSceLibcInternal/Err.cpp) (libSceLibcInternal) - NID `m6rjfL4aMcA` from the PS5 3.20 export inventory; it throws because the two Dinkumware versions differ. Dinkumware V6.00 (crt.vc12 f1d47126bf `xlocale` lines 1534-1564) returns the number of characters that converting from a copy of the state gives. microsoft/STL 219514876e (`xlocale` lines 850-891 and 1404-1407) returns the number of bytes and advances the caller's state. No source here shows which one the console's libc has.

## change [_ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m]

- [_ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m](../../../core/libs/prx/libSceLibcInternal/Err.cpp) (libSceLibcInternal) - NID `lynApfiP6Lw` from the PS5 3.20 export inventory; it throws because the two Dinkumware versions differ. Dinkumware V6.00 (crt.vc12 f1d47126bf `xlocale` lines 1267-1295) returns the number of characters that converting from a copy of the state gives. microsoft/STL 219514876e (`xlocale` lines 850-891 and 1162-1165) returns the number of bytes and advances the caller's state. No source here shows which one the console's libc has.
