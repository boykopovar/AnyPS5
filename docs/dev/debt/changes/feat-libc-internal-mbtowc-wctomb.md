# libSceLibcInternal

## add Unknown function info

- [_Mbtowc and _Wctomb](../../../core/libs/prx/libSceLibcInternal/Multibyte.cpp) (libSceLibcInternal) - the console module was not read. The `int(uint16_t*, const char*, size_t, void*)` and `int(char*, uint16_t, void*)` prototypes follow Dinkumware `_Mbtowc` and `_Wctomb` over PS5's 16-bit `wchar_t` and 16-byte `mbstate_t`, delegating UTF-8 decoding and encoding to `libc`'s `mbrtowc` and `wcrtomb` (`MB_CUR_MAX = 3`, BMP only, `EILSEQ = 86`). Unreadable or unwritable non-null guest pointers throw via `GuestCopyIn_nid_no_patch`/`GuestCopyOut_nid_no_patch` while preserving `errno`. Non-UTF-8 locale encodings and `_Cvtvec` parameter variants are not modelled. Closes when: compared with the console module.
