# libSceIcu

## add Functional

- [Resource bundle data readers](../../../core/libs/prx/libSceIcu/Resource.cpp) have no console ABI, availability or behavior verification. `res_read`, `res_getStringNoTrace`, `res_getAlias`, `res_getBinaryNoTrace`, `res_getIntVectorNoTrace`, `res_getResource`, `res_countArrayItems`, `res_getArrayItem`, `res_getTableItemByIndex` and `res_getTableItemByKey` forward to ICU 78.1 [uresdata.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/uresdata.cpp) over the 64-byte `ResourceData` and 20-byte `UDataInfo` layouts, unchanged from [ICU 68.2](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/uresdata.h). Returned pointers alias the caller's mapped bundle or pool storage. Null, unreadable or unwritable pointers on accessed paths throw `std::invalid_argument` and preserve `errno`. Closes when: compared with the console.
