# libSceAgcDriver

## add Functional

- [Store windows](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderAddressResources.cpp) (libSceAgcDriver) - store-table V#s are read on the CPU when a dispatch is recorded (`PendingWriteOverlaps` keeps the whole reach on GPU-written tables), assuming the guest does not overwrite those V#s from the CPU before GPU execution; PS4 (GCN) shaders also keep the whole reach. Narrowing applies only when every store goes through a dword-aligned table V# whose key is bounded by a non-loop-carried range (`SelectU32`/`ReadFirstLane` under the store's own mask, masks/shifts; find-bit results and re-read values are unbounded), a dominating scalar unsigned `key < bound` on a snapshot value (not a live shader memory read), or the holder buffer size, without 32-bit offset wrap and within 4096 records. Unbounded raw/index-only strides and swizzled V#s with an unknown required index, offset, or scalar-offset bound keep the whole reach.
