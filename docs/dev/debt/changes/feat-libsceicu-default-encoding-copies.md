# libSceIcu

## add Functional

- [UTF-16 to default-encoding copies](../../../core/libs/prx/libSceIcu/ConverterHelpers.cpp) `u_austrcpy` and `u_austrncpy` use the bundled ICU default converter, following [ICU 78.1](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustr_cnv.cpp). A positive count bounds both UTF-16 input units and encoded output bytes. The bounded copy can stop within an encoded character and writes one NUL if there is room. The unbounded copy throws if the result cannot be terminated within ICU's internal byte limit. The guest wrappers keep their null pointer and nonpositive count behavior. The console's default encoding and non-ASCII behavior need verification.
