# shader

## add Functional

- [Dynamic buffer descriptor tables](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) have no title or console compatibility evidence. `ANYPS5_BUFFER_DESCRIPTOR_TABLES=1` opts in on hosts exposing storage-buffer non-uniform indexing; the existing path remains the default. Only direct four-word V# columns used by raw 32-bit, noncoherent compute reads are admitted. Other access modes retain BDA. The CPU snapshots descriptors, so mutation of the descriptor heap during a dispatch is unsupported; a heap written by its own dispatch, an uncaptured key or an exhausted map/resource budget faults. Data rows outside the direct view size, alignment, mapping or 64-element binding budget retain BDA. Title timing and cross-vendor behaviour remain unverified.
