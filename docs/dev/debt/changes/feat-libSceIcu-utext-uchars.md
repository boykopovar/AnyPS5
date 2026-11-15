# libSceIcu

## remove `o9+yMOok2Zs`

## remove `sEKpfqARKPI`

## change [ICU UText exports]

- [ICU UText exports](../../../core/libs/prx/libSceIcu/UText.cpp) are unverified on a console. `utext_setup`, `utext_close` and the `utext_openUChars` provider follow ICU 68.2 [utext.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utext.cpp) (577-705, 750-829, 2347-2642) and the `UText` layout of its `utext.h`; the NIDs of `utext_setup` and `utext_openUChars` come from the 3.20 inventory. The extra storage of a heap `UText` starts 144 bytes into its block, assuming a 16-byte aligned `std::max_align_t`. A null status, and an open `UText` with a null `pFuncs`, throw where ICU dereferences them; guest memory is accessed unchecked. The accessors (`utext_next32` and the others) and the UTF-8, `UnicodeString`, `Replaceable` and `CharacterIterator` providers still throw. Closes when: those are implemented and compared with the console.
