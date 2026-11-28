# libc

## add Inventory exports declared elsewhere

- [_Getctype, _Getcoll, _Getcvt, _Getdateorder, _Strcoll and _Strxfrm](../../../core/libs/prx/libc/src/CLocaleHelpers.cpp) (libc) follow the `libc.prx` bundled with PPSA16064 (disassembly), C locale only. `_Getctype` returns `{_Getpctype(), 0}` in rax:rdx, `_Getcvt` fills a 40-byte block through the hidden return pointer (`1` at +0, `1` at +4, zeros from +8; whether the structure is larger is not known) and `_Getdateorder` returns 2. `_Getcoll` is a bare `ret` there, so the register pair it returns is whatever the caller left; this returns zeros. `_Strcoll` returns the difference of the first differing bytes, or `-1`/`1` when one range is a prefix of the other, and `_Strxfrm` copies the source range only when it fits the destination range, returning its length either way. The collation argument is ignored.
