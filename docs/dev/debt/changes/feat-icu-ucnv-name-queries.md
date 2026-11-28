# libSceIcu

## add Functional

- [ucnv_countAvailable, ucnv_getAvailableName, ucnv_countAliases, ucnv_getAlias, ucnv_countStandards and ucnv_getStandardName](../../../core/libs/prx/libSceIcu/ConverterAliases.cpp) have no console ABI or behavior verification. Their signatures, status behavior and borrowed string lifetime follow bundled ICU 78.1 [ucnv_io.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ucnv_io.cpp). Status and names are read through checked guest access; invalid guest memory throws through `NotImplemented_nid_no_patch`. Returned strings are ICU-owned host storage that is guest-visible in AnyPS5's native-process model. The reduced bundled ICU data package omits `cnvalias.icu`, so these queries report zero converters, zero standards or ICU's data-loading error unless compatible common data is supplied. Closes when: compared with the console and its converter alias data is bundled.
