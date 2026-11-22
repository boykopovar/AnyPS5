# libSceJxr

## add Functional

- [WMP encoder and decoder guest pointer validation](../../../core/libs/prx/libSceJxr/src/Wmp.cpp) - console behaviour on inaccessible or read-only output slots and buffers in `PKImageEncode_Create_WMP`, `PKImageDecode_Create_WMP`, and the WMP encoder/decoder `Release`, `GetSize`, `GetRawStream`, and `GetColorContext` callbacks is unmeasured. Closes when: console behaviour on invalid guest pointers in these WMP entry points is measured.
