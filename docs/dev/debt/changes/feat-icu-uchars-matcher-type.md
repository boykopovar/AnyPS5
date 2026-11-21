# libSceIcu

## add Functional

- [UCharsDictionaryMatcher::getType](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) (`6KsOlzLtoaE`) - the console ABI and return value are not verified on hardware. The 3.20 NID was [matched to the ICU 68 symbol](https://github.com/boykopovar/AnyPS5/pull/14016); the no-argument const method in [ICU 68.2](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/dictionarydata.cpp#L24-L25) returns the UChars trie tag 1 [without reading the receiver](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/dictionarydata.cpp#L41-L43). This does not implement dictionary loading or word matching.
