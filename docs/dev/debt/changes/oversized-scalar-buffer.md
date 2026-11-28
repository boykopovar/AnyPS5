# shader

## add Functional

- [Oversized scalar buffers](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) use the existing GPU descriptor address path when known descriptor bits prove that stride times record count exceeds `UINT32_MAX`. Unknown sizes and vector accesses retain the storage-buffer path and may still exceed its device range limit. The address path's scalar bounds checks are unchanged; the selection is covered by synthetic IR tests and Teardown PPSA15246 on Windows/NVIDIA, not measured on a console. Large registered allocations may still require a higher `APS5_HOST_IMPORT_MIB` budget to avoid whole-region staging allocations.
