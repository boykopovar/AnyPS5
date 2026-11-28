# shader

## add Functional

- A [waterfall loop](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) over per-lane T#s becomes an [image table](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) keyed by each lane's buffer index only when every T# dword comes from `v_readlane_b32` or `v_readfirstlane_b32` and the EXEC writing the results proves each equals the lane's own VGPR. The load must be `buffer_load_dword*` (IDXEN, immediate VOFFSET/SOFFSET) from one SRT-evaluable V#; other per-lane T#s, S#s, stores or atomics throw. A table V# with zero records, base or format binds no record; with records it faults when selected unless unswizzled, without `ADD_TID`, with `OOB_SELECT` 0 or 1, aligned records and, under `OOB_SELECT` 0, a stride past the last dword; keys past the 65536-record snapshot fault. An implicit-derivative or LOD-query table faults when two snapshot keys map to different records; per-quad check open. Not measured on hardware.
