# libSceIcu

## change [Locale writing direction]

- [Locale writing direction](../../../core/libs/prx/libSceIcu/RightToLeft.cpp) uses the module default for a null locale. For a locale with an implied script outside the common-language table, it uses the registered ICU likely-subtags data and throws if that data is unavailable. Direction follows [ICU 78.1](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/loclikely.cpp#L348-L386). The ICU 68 export name also appears in the inventory. [ICU 68.2](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/loclikely.cpp#L1301-L1348) uses an eight-byte language buffer and older script direction data. Closes when: compared with a console.

## change [Character property]

- [Character property](../../../core/libs/prx/libSceIcu/Properties.cpp) exports forward to ICU 78.1 with its compiled-in tables plus the pinned `uemoji.icu`, `ulayout.icu`, `nfkc.nrm` and `nfkc_cf.nrm` from the [3rdparty/icu](https://github.com/unicode-org/icu/tree/049e0d6a4206/icu4c/source/data/in) submodule, embedded in the common data entry point. Values follow Unicode 17; the console's ICU 68 has Unicode 13. [Case mapping](../../../core/libs/prx/libSceIcu/CaseMapping.cpp) `u_strToLower` and `u_strToUpper` resolve a null locale through `uloc_getDefault` as ICU does. Prototypes follow the ICU 78.1 headers; null pointers ICU dereferences and lengths below -1 throw `std::invalid_argument`. Closes when: compared with the console.
