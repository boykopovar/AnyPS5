# libSceLibcInternal

## add Functional

- [messages<char> and messages<wchar_t> open, get, close and do_get](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal, NIDs `U2t+WvMQj8A`, `XxsPrrWJ52I`, `EeBQ7253LkE`, `vbgCuYKySLI`, `TPq0HfoACeI`, `Ea+awuQ5Bm8`, `GGoH7e6SZSY`, `UM6rGQxnEMg`) follow the Microsoft STL [`xlocmes`](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocmes#L38-L91); the console's `libSceLibcInternal.sprx` was not read. `open`, `get` and `close` call the `do_*` virtuals; `do_get` ignores catalog, set and message and copies the default as [`_Construct_lv_contents`](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xstring#L2461-L2479) does (capacity 15 or `size | 15`; 7 or `size | 7` for wide). `max_size()` is assumed to be `SIZE_MAX - 1` (wide: `SIZE_MAX / 2 - 1`); a longer default, a null facet, vtable, slot, result or default, or heap capacity without storage throws. Only `messages<char>` exports a vtable.

## remove [_ZNKSt8messagesIcE3getEiiiRKSs]

## remove [_ZNKSt8messagesIcE4openERKSsRKSt6locale]

## remove [_ZNKSt8messagesIcE5closeEi]

## remove [_ZNKSt8messagesIcE6do_getEiiiRKSs]

## remove [_ZNKSt8messagesIwE3getEiiiRKSbIwSt11char_traitsIwESaIwEE]

## remove [_ZNKSt8messagesIwE4openERKSsRKSt6locale]

## remove [_ZNKSt8messagesIwE5closeEi]

## remove [_ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE]

# libc

## change [messages<char> vtable]

- [messages<char> vtable](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc, NID `FnEnECMJGag`) is nine `uintptr_t` slots matching `st_size` 72 on title `libc.prx` from PPSA26344. Offset-to-top and typeinfo stay zero, unlike the console's relocated entries, and `messages<char>::_Getcat` (NID `96Ev+CE1luE`) still throws, so nothing here builds the facet. Slots 2 through 5 hold the `collate<char>` destructors and `_Incref`/`_Decref`; slots 6 through 8 hold `do_open` (returns -1), `do_get` (copies the default by the libSceLibcInternal `messages` rules) and `do_close` (does nothing), as the Microsoft STL [`xlocmes`](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocmes#L84-L94) does.
