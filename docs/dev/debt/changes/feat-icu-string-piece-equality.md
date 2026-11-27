# libSceIcu

## add Functional

- [StringPiece equality](../../../core/libs/prx/libSceIcu/StringPiece.cpp) - the guest layout, one-byte `UBool` result and comparison are based on the [ICU 68.2 class](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/unicode/stringpiece.h) and [operator](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/stringpiece.cpp), not console measurements. Unreadable inputs needed for a comparison and equal negative lengths throw rather than invoking undefined host behavior.
