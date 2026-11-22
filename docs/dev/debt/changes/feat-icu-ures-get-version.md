# libSceIcu

## add Functional

- [Resource version exports](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) (libSceIcu) - `ures_getVersionNumberInternal`, `ures_getVersionNumber`, `ures_getVersion` and `ures_getVersionByKey` follow ICU 68.2 and ICU 78.1 ([uresbund.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresbund.cpp)). `ures_getVersionNumberInternal` and `ures_getVersionNumber` read `fVersion` at ICU 68 offset 16 through checked guest copies. When `fVersion` is null or requires bundle string fallback lookup, in `ures_getVersionByKey` for non-null bundles, or when `versionInfo` is non-null and unwritable, they throw `NotImplemented_nid_no_patch` until `ures_open` is implemented. Unverified on a console. Closes when: compared with the console.
