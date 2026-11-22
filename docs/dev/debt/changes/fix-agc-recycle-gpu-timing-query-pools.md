# libSceAgcDriver

## add Functional

- [Query pools on MoltenVK](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Recorder.cpp) (libSceAgcDriver): MoltenVK 1.4.2 (Vulkan SDK 1.4.363.0) reads a query pool in a Metal command buffer completion handler without holding a reference to it, and that handler can run after the batch's fence signals, so destroying the pool once the batch completes, as Vulkan allows, makes MoltenVK read freed memory (`agc_driver_profiled_dispatch_run` crashed in `MVKCommandEncoder::finishQueries` in 11 of 15 runs on an Apple M5 Pro). MoltenVK retains the pools from KhronosGroup/MoltenVK#2839, not in a release yet. The recorder keeps its timestamp pools for reuse in every mode, so GPU timing no longer destroys one per batch; the occlusion sample pools a batch owns are still destroyed when it completes, and a pool the 64-pool spare list cannot take is still destroyed.
