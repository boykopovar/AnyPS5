# libSceIcu

## add Functional

- [Default-encoding to UTF-16 copies](../../../core/libs/prx/libSceIcu/Invariant.cpp) `u_uastrcpy` and `u_uastrncpy` use the bundled ICU default converter, following [ICU 78.1](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustr_cnv.cpp). A positive count bounds both input bytes and output code units. The bounded export writes a NUL if there is room and preserves the existing zero and negative count handling. The unbounded export rejects input beyond ICU's signed length range or output that cannot be terminated within ICU's internal capacity. The console's default encoding and non-ASCII behavior remain unverified.
