# libSceIcu

## add Functional

- [Normalizer2Impl::combine](../../../core/libs/prx/libSceIcu/NormalizerCombine.cpp) (libSceIcu) - not measured on a console. It is ICU 68.2's [combine](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/normalizer2impl.cpp#L1004-L1049) and, like it, reads the guest's composition list without bounds: a null list, a list whose last entry lacks `COMP_1_LAST_TUPLE`, a trail whose first key unit is 0x8000 or more (a negative trail with bit 14 set, or some trails above 0x10FFFF), or a trail of U+3400 or more whose first key unit equals the key of a two-unit entry (up to U+10FFFF, the entry of a trail from U+1A0D to U+1E3F, such as U+1B35 in ICU's NFC data: ICU 68.2's composePair(U+1B05, U+4D4D8) returns U+65B35; above U+10FFFF, any two-unit entry, such as U+0041 for trail 0x01990403) can read past the list, where ICU 68.2 reads the same memory.
