# libSceIcu

## add Functional

- [Normalizer2 accessors](../../../core/libs/prx/libSceIcu/Normalizer2Accessors.cpp) are unverified on a console. The `Normalizer2` and `Normalizer2Factory` getters, the `Normalizer2` base queries and the `Normalizer2Impl` queries call ICU 78.1 (Unicode 17; the console's ICU 68 has Unicode 13) and return host ICU 78.1 objects, so title code that reads their members inline with the ICU 68 layout gets wrong values. `getImpl` on a normalizer without normalization data is undefined in ICU and throws. `addLcccChars` takes a set from this library's `uset_*` exports. The status, length and buffer go through `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch`; ICU reads the `isNormalizedUTF8` text directly. A null or inaccessible argument or a null `this` throws where the console would fault. Closes when: compared with the console.
