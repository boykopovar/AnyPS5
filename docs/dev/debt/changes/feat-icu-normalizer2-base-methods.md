# libSceIcu

## add Functional

- The [Normalizer2 base method](../../../core/libs/prx/libSceIcu/Normalization.cpp) `composePair` returns `U_SENTINEL` for every pair, as specified by [ICU 68.2](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/normalizer2.cpp#L69-L71). Its guest ABI and behavior have not been verified on a console.
