# libSceIcu

## change [ICU UText exports]

- [ICU UText exports](../../../core/libs/prx/libSceIcu/UText.cpp) are unverified on a console. `utext_close` follows ICU 78.1 [utext.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/utext.cpp#L668-L708), releasing provider resources through the guest callback and framework storage through the guest ICU allocator. Its public layout follows [utext.h](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/unicode/utext.h#L1194-L1520). The ICU 68.2 close logic agrees; current-firmware availability of its older inventory NID is unproven. Other UText providers and `utext_setup` remain unimplemented. Closes when: those are implemented and compared with the console.
