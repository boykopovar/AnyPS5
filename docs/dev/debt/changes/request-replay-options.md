# shader

## remove [Captured shader target robustness]

## change [Shader request captures]

- [Shader request captures](../../../core/shader/recompiler/ControlFlow/src/RequestSerializer.cpp) - format 64 preserves the buffer robustness, memory barrier, mesh window/inline, host f16 NaN and converted/layer-row sampling options. Earlier captures omit these options and restore their false defaults; their original values cannot be recovered from the blob alone. Driver-derived split state is still not serialized, so a successful standalone replay does not prove an exact native-pipeline match.
