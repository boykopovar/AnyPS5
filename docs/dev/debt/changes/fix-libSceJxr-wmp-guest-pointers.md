# libSceJxr

## add Functional

- [WMP encoder and decoder guest pointer validation](../../../core/libs/prx/libSceJxr/src/Wmp.cpp) - console behaviour on inaccessible or read-only output slots, pixel buffers, parameter blocks, rectangles, and colour-context buffers in `PKImageEncode_Create_WMP`, `PKImageDecode_Create_WMP`, and the WMP encoder/decoder `Initialize`, `WritePixels`, `WritePixelsBanded`, `Release`, `GetSize`, `GetRawStream`, `Copy`, and `GetColorContext` callbacks is unmeasured. Opaque `encoder`, `decoder`, and `WMPStream` struct bodies/callbacks, `EncodeTranscode` parameter blocks, and `EncodeSetDescriptiveMetadata`/`DecodeGetDescriptiveMetadata` struct bodies and nested string pointers remain unvalidated beyond null checks. Closes when: console behaviour on invalid guest pointers in WMP entry points is measured and remaining internal struct validation is completed.
