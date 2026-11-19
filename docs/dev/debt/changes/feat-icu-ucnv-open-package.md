# libSceIcu

## add Functional

- [`ucnv_openPackage`](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) is unverified on a console, and this build disables ICU file I/O, so filesystem converter packages report `U_FILE_ACCESS_ERROR`; application-data packages remain usable. Its signature, name parsing, lookup, status and ownership follow bundled ICU 78.1 [ucnv.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ucnv.cpp#L86-L90) and [ucnv_bld.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ucnv_bld.cpp#L919-L969); the console name comes from ICU 68.2. Guest strings and status use checked copies; invalid memory throws instead of reproducing native invalid access. The opaque host converter is consumed and released by the host-backed exports in #18400. Closes when: compared with the console and filesystem package loading is implemented.
