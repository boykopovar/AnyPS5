# libSceAgcDriver

## add Unknown function info

- [Zero vertex fetches](../../../core/libs/prx/libSceAgcDriver/Graphics/include/VertexInput.hpp) (libSceAgcDriver) - the constant-zero cases of typed vertex fetches are not measured on gfx1013. The range rules follow the RDNA 2 measurements linked in #2241 and #8299: strided `OOB_SELECT` 2 with `NUM_RECORDS` 0, and 3 with fewer records than the typed fetch bytes, read zeros regardless of the index. They use a zero-stride, four-component host binding so Vulkan does not supply a default W of 1 for a narrow format. A null or unaligned address is ignored only for these provably zero fetches or the existing zero-stride zero cases. Descriptor swizzle and unsupported flags still throw; mixed in-range/out-of-range zero-stride select-1 fetches retain their existing limitation.
