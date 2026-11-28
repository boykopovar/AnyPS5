# libc

## change [num_put<char> vtable]

- [num_put<char> vtable](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc, NID `1kZFcktOm+s`) is fourteen `uintptr_t` slots matching `st_size` 112 on the title `libc.prx` from PPSA16833 (offset-to-top, typeinfo, two destructors, `_Incref`/`_Decref`, and eight `do_put` virtuals). Slots 2 through 13 hold the functions of the vtable that `num_put<char>::_Getcat` installs, so a facet a title builds on the exported object formats as the `_Getcat` facet does and has its gaps; offset-to-top and typeinfo remain zero, unlike the console's relocated entries. The executables of PPSA05510, PPSA01670 and PPSA02929 import this vtable ([#268](https://github.com/boykopovar/AnyPS5/pull/268), [#272](https://github.com/boykopovar/AnyPS5/pull/272), [#265](https://github.com/boykopovar/AnyPS5/pull/265)).

## change [codecvt<char,char,_Mbstatet> vtable]

- [codecvt<char,char,_Mbstatet> vtable](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc, NID `aK1Ymf-NhAs`) is thirteen `uintptr_t` slots matching `st_size` 104 on title `libc.prx` from PPSA26344 and on `libSceLibcInternal.sprx` (offset-to-top, typeinfo, two destructors, `_Incref`/`_Decref`, and seven conversion virtuals). Slots 2 through 12 hold the functions of the vtable that `codecvt<char>::_Getcat` installs (`noconv` conversions, `length` the shorter range); offset-to-top and typeinfo remain zero, unlike the console's relocated entries. The executables of PPSA05510 and PPSA21564 import this vtable ([#268](https://github.com/boykopovar/AnyPS5/pull/268), [#271](https://github.com/boykopovar/AnyPS5/pull/271)).
