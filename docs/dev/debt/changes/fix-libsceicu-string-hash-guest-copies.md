# libSceIcu

## add Functional

- [String hash reads](../../../core/libs/prx/libSceIcu/StringHash.cpp) (libSceIcu) - `ustr_hashUCharsN`, `ustr_hashCharsN` and `ustr_hashICharsN` copy through checked copies only the units ICU samples, at most 63, and hash them with ICU, which gives the same value without reading units ICU skips. An unreadable sampled unit throws `std::invalid_argument` where ICU dereferences it, so this is a host-safety fallback with no console behaviour to match.
