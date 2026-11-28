# libSceJxr

## add Functional

- [WMP metadata setter guest pointer validation](../../../core/libs/prx/libSceJxr/src/EncoderMetadata.hpp) - console behaviour on inaccessible, read-only or overflowing `encoder` and `metadata` guest pointers in `PKImageEncode_SetEXIFMetadata_WMP`, `PKImageEncode_SetGPSInfoMetadata_WMP`, `PKImageEncode_SetIPTCNAAMetadata_WMP`, `PKImageEncode_SetPhotoshopMetadata_WMP`, `PKImageEncode_SetXMPMetadata_WMP`, and WMP colour-context metadata updates is unmeasured. Closes when: console behaviour on invalid guest pointers in these metadata setters is measured.
