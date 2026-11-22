# libSceJxr

## add Functional

- [WMP encoder and decoder guest pointer validation](../../../core/libs/prx/libSceJxr/src/Wmp.cpp) - console behaviour on inaccessible or read-only output slots, pixel buffers, parameter blocks, rectangles, and colour-context buffers in `PKImageEncode_Create_WMP`, `PKImageDecode_Create_WMP`, and WMP encoder/decoder callbacks is unmeasured. Pixel-buffer probing uses the row-width-aware span `(effectiveRows - 1) * stride + rowBytes` from the active pixel format and width (falling back to `lines * stride` with zero-stride rejection only when the format is unknown). Opaque `encoder`/`decoder`/`WMPStream` struct bodies/callbacks, `EncodeTranscode` parameters, and descriptive-metadata struct bodies/strings remain unvalidated beyond null checks. Closes when: console behaviour on invalid WMP guest pointers is measured and remaining internal struct validation is completed.
