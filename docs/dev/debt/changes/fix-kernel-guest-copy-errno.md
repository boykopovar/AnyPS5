# libkernel

## add Functional

- [guest destination validation](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (`sceKernelBatchMap2`, `sceKernelStopUnloadModule`) - validates guest destination pointer writability upfront via `GuestArena::HostWrite` paired with `GuestWritable_nid_no_patch` before modifying mappings or attempting unload; console behavior when unwritable output pointers are passed with otherwise valid batch-map entries or module handles is unmeasured on hardware. Closes when: measured on hardware. Labels: console, tested.
