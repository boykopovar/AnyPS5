# libSceAgcDriver

## change [GPU descriptor heap]

- [GPU descriptor heap](../../../core/libs/prx/libSceAgcDriver/Graphics/src/BindlessHeap.cpp) (`APS5_GPU_HEAP`) - opt-in, needs `VK_EXT_descriptor_heap` and timeline semaphores; without the extension it has no effect. Heap mode has no per-draw path: a stage that needs more image slots than the heap holds, whose module keeps a binding the heap does not map (`RequireHeapStageMappable`) or whose plan is ineligible, and a draw whose stages come out mixed, fail at the draw or dispatch, not at registration (only `APS5_GPU_HEAP_EXCLUDE` sends stages per-draw). With `APS5_HEAP_SLOT_CAP` far below the default, stages that each fit can still exhaust the heap between their compile and their bind. Heap stages report no BDA fault per use, so the address draw reuse test skips its fault-per-use case with `--heap-stages`. A reused address-based draw whose moved read-only buffer cannot be viewed in place is built again.
