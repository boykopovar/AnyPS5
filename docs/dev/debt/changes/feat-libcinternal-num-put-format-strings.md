# libSceLibcInternal

## remove `xlgA01CQtBo`

## remove `w11G58-u4p8`

## remove `WheFSRlZ9JA`

## remove `T07KcAOlIeU`

## add Functional

- [num_put _Ifmt and _Ffmt](../../../core/libs/prx/libSceLibcInternal/NumPutFormat.cpp) (libSceLibcInternal, NIDs `xlgA01CQtBo`, `WheFSRlZ9JA`, `w11G58-u4p8` and `T07KcAOlIeU`, `char` and `wchar_t`) - the firmware module and console execution are unverified. The four follow the `libc.prx` bundled with PPSA21564 (`0x97a60`, `0xa9510`, `0x98080` and `0xa9b30`), read in disassembly, where the `char` and `wchar_t` bodies are identical: `_Ifmt` writes `%`, `+` for showpos, `#` for showbase, the specifier's first character or `ll` for `L`, then `o`, `x`, `X` or the specifier's second character by the base field; `_Ffmt` writes `%`, `+` for showpos, `#` for showpoint, `.*` unless the float field is hexfloat, the length modifier unless it is zero, then `f` for fixed in either case, `a`, `e` or `g`, uppercase with the uppercase flag; both end the text with a zero byte and return the buffer. An unreadable specifier or an unwritable buffer throws where the module faults. Closes when: compared with the firmware `libSceLibcInternal.sprx` or measured on a console.
