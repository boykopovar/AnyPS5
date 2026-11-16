# libSceIcu

## remove `yc1XEzBopCw`

## remove `o9+yMOok2Zs`

## remove `sEKpfqARKPI`

## add Functional

- [UText providers](../../../core/libs/prx/libSceIcu/TextProviders.cpp) have no console ABI or behavior verification. `utext_setup`, `utext_openUChars`, `utext_openConstUnicodeString` and their `const UChar*` and `UnicodeString` providers follow ICU 68.2 [utext.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utext.cpp) over the 144-byte `UText` of [Text.cpp](../../../core/libs/prx/libSceIcu/Text.cpp) and the 64-byte `UnicodeString` of [UnicodeString.cpp](../../../core/libs/prx/libSceIcu/UnicodeString.cpp). A heap `UText` comes from `uprv_malloc` with its extra space at byte 144, the `ExtendedUText` offset under a 16-byte `max_align_t`. `utext_close` still throws, so these texts are not closed through it. A null text, string or provider table that ICU dereferences throws `std::invalid_argument`, and a null `UErrorCode` throws `NotImplemented_nid_no_patch`. Closes when: compared with the console.
