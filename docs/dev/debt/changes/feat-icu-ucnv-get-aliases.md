# libSceIcu

## add Functional

- [ucnv_getAliases](../../../core/libs/prx/libSceIcu/ConverterAliases.cpp) has no console ABI or behavior verification. Its signature, status behavior, alias ordering and borrowed string lifetime follow bundled ICU 78.1 [ucnv_io.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ucnv_io.cpp#L913-L936). The alias and status are read through checked guest access, and the complete pointer array is validated before one checked copy; invalid guest memory throws through `NotImplemented_nid_no_patch`. ICU owns the returned strings for the process lifetime, and their readable host mappings are guest-visible in AnyPS5's native-process model. The reduced bundled ICU data package omits `cnvalias.icu`, so alias queries report ICU's data-loading error unless compatible common data is supplied before alias initialization. Closes when: compared with the console and its converter alias data is bundled.
