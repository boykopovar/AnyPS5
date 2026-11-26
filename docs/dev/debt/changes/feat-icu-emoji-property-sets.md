# libSceIcu

## change [Code point sets]
- [Code point sets](../../../core/libs/prx/libSceIcu/Set.cpp) use the bundled ICU 78.1 `UnicodeSet` for range membership, union and string items. `uset_getItem` enumerates ranges first and then UTF-16 strings. Firmware 13.42 exports these set functions ([#11013](https://github.com/boykopovar/AnyPS5/pull/11013#issuecomment-6401310351)). The behavior of the console's set operations and the Unicode version of its emoji sequences are unmeasured.
