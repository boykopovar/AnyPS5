# libSceAgcDriver

## change [VertexBufferReadSize]

- [VertexBufferReadSize](../../../core/libs/prx/libSceAgcDriver/Graphics/include/VertexInput.hpp) and the [vertex staging copy](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp) (libSceAgcDriver) - range rules follow RDNA 2 gfx1036 measurements in #2241, not console gfx1013 measurements. Out-of-range selects 0/1 return four zeros, including W; select 2 ignores the index limit and select 3 checks the typed fetch's byte extent. Out-of-range copies bypass the draw-snapshot cache to avoid reusing another draw's bytes or caching padded zeros, and flush their valid prefix before reading GPU writes. Zero-stride descriptors use separate byte-count rules.

## add Functional

- [Strided select-2/3 vertex fetches](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/IndirectDraw.cpp) with a nonzero live range resolve indirect records on the CPU: the descriptor does not bound the fetched vertex indices, so its extent cannot size a copied input for unknown draw counts. Keeping these draws on the GPU requires an input binding that covers the actual fetches, including inputs unavailable for host import and pending GPU producers. Known-zero fetches retain the GPU-side route.

- [Zero vertex fetches](../../../core/libs/prx/libSceAgcDriver/Graphics/include/VertexInput.hpp) (libSceAgcDriver) - the constant-zero cases of typed vertex fetches are not measured on gfx1013. The range rules follow the RDNA 2 measurements linked in #2241 and #8299: strided `OOB_SELECT` 2 with `NUM_RECORDS` 0, and 3 with fewer records than the typed fetch bytes, read zeros regardless of the index. They use a zero-stride, four-component host binding so Vulkan does not supply a default W of 1 for a narrow format. A null or unaligned address is ignored only for these provably zero fetches or the existing zero-stride zero cases. Descriptor swizzle and unsupported flags still throw; mixed in-range/out-of-range zero-stride select-1 fetches retain their existing limitation.
