# shader

## remove [Captured shader target robustness]

## change [Shader request captures]

- [Shader request captures](../../../core/shader/recompiler/ControlFlow/src/RequestSerializer.cpp) - format 66 preserves the buffer robustness, memory barrier, mesh window/inline, host f16 NaN and converted/layer-row sampling options. Earlier upstream captures omit these options and restore their false defaults; their original values cannot be recovered from the blob alone. Upstream format 64 stores driver-derived split state and remains readable. Earlier draft captures from this branch also used the number 64 for a different layout. Some byte sequences fit both layouts, so replay those draft captures with their original build.
