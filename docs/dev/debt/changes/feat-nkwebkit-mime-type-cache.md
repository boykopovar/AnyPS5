# libSceNKWebKit

## add Functional

- [WebCore::MIMETypeCache](../../../core/libs/prx/libSceNKWebKit/src/MIMETypeCache.cpp) (libSceNKWebKit) - the copy constructors, the base destructor, the copy assignment, `canDecodeType`, `supportedTypes`, `addSupportedTypes` and `supportsContainerType`, not compared with a console module. They follow `MIMETypeCache.cpp` of WebKit at `9d2532291ea`. The object has the layout of the header of that revision (40 bytes: the virtual table pointer, then two `std::optional` of a hash table, each a table pointer followed by its flag) and the virtual table has the header's nine slots; the tables are WTF's (metadata before the buckets, triangular probing, `ASCIICaseInsensitiveHash`). Derived, not measured. Closes when: compared with a console module.
- [MIMETypeCache gaps](../../../core/libs/prx/libSceNKWebKit/src/MIMETypeCache.cpp) (libSceNKWebKit) - the copy constructors install a virtual table of this port whose deleting destructor slot is still the `D0` stub and whose `isStaticContainerType` and `initializeCache` are the base versions, which are not exported. `addSupportedTypes` with a null `WTF::String` in the list throws `std::invalid_argument` where the source hashes a null pointer. The codec list comes from `ContentType::parameter`, `ContentType::codecs` not being exported. Closes when: `D0` is implemented and a console module is read for the null string.

## remove `_ZN7WebCore13MIMETypeCache13canDecodeTypeERKN3WTF6StringE`

## remove `_ZN7WebCore13MIMETypeCache14supportedTypesEv`

## remove `_ZN7WebCore13MIMETypeCache17addSupportedTypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE`

## remove `_ZN7WebCore13MIMETypeCache21supportsContainerTypeERKN3WTF6StringE`

## remove `_ZN7WebCore13MIMETypeCacheC1ERKS0_`

## remove `_ZN7WebCore13MIMETypeCacheC2ERKS0_`

## remove `_ZN7WebCore13MIMETypeCacheD2Ev`

## remove `_ZN7WebCore13MIMETypeCacheaSERKS0_`
