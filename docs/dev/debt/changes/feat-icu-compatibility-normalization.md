# libSceIcu

## change [Normalization]
- [Normalization](../../../core/libs/prx/libSceIcu/Normalization.cpp) is unverified on a console. Host ICU 78.1 uses Unicode 17, while console ICU 68 uses Unicode 13. NFC, NFD and `unorm_compare` use the built-in NFC data. NFKC, NFKD, NFKC_Casefold and FC_NFKC_Closure use compatibility data from the pinned ICU submodule. `unorm2_getInstance` accepts nfc, nfkc and nfkc_cf without a package; other data names and packages throw when their data is unavailable. A filtered normalizer snapshots the guest range set and borrows its base normalizer. Closing a borrowed singleton throws, as do a null error pointer and a null normalizer where ICU dereferences it. Closes when: compared with the console.
