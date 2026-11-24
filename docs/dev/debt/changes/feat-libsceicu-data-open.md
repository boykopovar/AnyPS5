# libSceIcu

## add Functional

- [`udata_open` and `udata_openChoice`](../../../core/libs/prx/libSceIcu/DataOpen.cpp) use the bundled ICU's [data loader](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/udata.cpp). The wrappers pass an empty type to the loader for a null guest type because the loader's time-zone check dereferences the native type. The guest callback still receives the original type. The bundled build disables file I/O and has a reduced data package. Data files from the guest filesystem and PS5 firmware data are unavailable. The host tests exercise the bundled data and callback ABI. The exports still need PS5 verification.
