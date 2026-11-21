# shader

## add Functional

- [Wave LDS ordering on NVIDIA's proprietary driver before 596](../../../core/shader/recompiler/SpirvBackend/src/SpirvFlowEmitter.cpp) (shader recompiler) - below driver version 596.0 ([SubgroupBarrier.hpp](../../../core/libs/prx/libSceAgcDriver/Execution/include/SubgroupBarrier.hpp)) a wave's LDS accesses are ordered by a subgroup `OpMemoryBarrier` alone, relying on a warp's lanes issuing LDS together, which Vulkan does not guarantee. With the subgroup `OpControlBarrier`, an RTX 3060 failed [LdsTreeReduction.cpp](../../../core/libs/prx/libSceAgcDriver/tests/execution/LdsTreeReduction.cpp) on 560.94 and 572.16 and [DivergentDeviceAtomicBarrier.cpp](../../../core/libs/prx/libSceAgcDriver/tests/execution/DivergentDeviceAtomicBarrier.cpp) on 572.16; the memory barrier passes both. Drivers 573 to 595 are unmeasured on Volta and later, and Linux 580.178.04 on Pascal, which passed with the control barrier, now takes the memory barrier (#10758).
