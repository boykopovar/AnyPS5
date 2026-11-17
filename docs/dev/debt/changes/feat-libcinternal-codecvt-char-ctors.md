# libSceLibcInternal

## remove _ZNSt7codecvtIcc9_MbstatetEC1ERKSt8_Locinfom

## remove _ZNSt7codecvtIcc9_MbstatetEC2ERKSt8_Locinfom

## remove _ZNSt7codecvtIcc9_MbstatetEC2Em

## remove _ZNSt7codecvtIcc9_MbstatetED0Ev

## remove _ZNSt7codecvtIcc9_MbstatetEC1Em

## add Functional

- [codecvt<char, char, _Mbstatet> constructors and deleting destructor](../../../core/libs/prx/libSceLibcInternal/CodecvtChar.cpp) (libSceLibcInternal, NIDs `XhwSbwsBdx0`, `e5Hwcntvd8c`, `3YCLxZqgIdo`, `4qHwSTPt-t8`, `2TYdayAO39E`) are unverified on a console: the constructors validate non-null `self` (and non-null `_Locinfo` for the `_Locinfo` overloads), write only the `0x10`-byte `GuestLocale::Facet` header (`&_ZTVSt7codecvtIcc9_MbstatetE_nid_postfix[2]` and 64-bit `refs`), and `D0` frees `self` through `_ZdlPv_nid_postfix`, matching Microsoft STL [xlocale](https://github.com/microsoft/STL/blob/770da5aa457af11f0a796600e1aa6f7f6fce14a6/stl/inc/xlocale#L673-L693).
