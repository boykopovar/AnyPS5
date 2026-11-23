# libSceIcu

## add Functional

- [FilteredNormalizer2 code point queries](../../../core/libs/prx/libSceIcu/FilteredNormalizerQueries.cpp) are unverified on a console. `FilteredNormalizer2::composePair`, `isInert` and `hasBoundaryBefore` call ICU 78.1 (Unicode 17; the console's ICU 68 has Unicode 13) on a normalizer from `unorm2_openFiltered`. Calling them on any other normalizer is undefined in ICU and throws here, as does a null `this`. Closes when: compared with the console.
