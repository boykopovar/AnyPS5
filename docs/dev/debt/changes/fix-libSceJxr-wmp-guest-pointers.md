# libSceJxr

## add Functional

- [WMP guest pointer validation](../../../core/libs/prx/libSceJxr/src/Wmp.cpp): console behaviour on invalid or read-only output slots, pixel buffers, parameter blocks, rectangles and colour-context buffers is unmeasured. Pixel probes use `(effectiveRows - 1) * stride + rowBytes` for the active format and encoder width or decoder ROI width; rectangle width is ignored by decoding. Negative decoder rectangle heights are refused before calling jxrlib; the console error result remains unmeasured. Before its first band, copied decoder settings use jxrlib `WMPhotoValidate`, which reads and rewinds the header; later bands use the normalized ROI. Unknown formats retain the `lines * stride` fallback and zero-stride refusal. Opaque encoder/decoder/stream bodies and callbacks, transcode parameters and descriptive metadata remain unvalidated beyond null checks. Closes when: console pointer-error behaviour is measured and internal struct validation is completed.
