# libSceAgcDriver

## add Opt-in draw paths

- [Guest clock pause for compute pipeline builds](../../../core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp) (`APS5_PAUSE_CLOCK_FOR_PIPELINES`) - opt-in. While a queue builds a compute pipeline, or waits for one that a prebuild thread is building, the guest's monotonic clocks (`clock_gettime` monotonic ids, `sceKernelGetProcessTime` and `sceKernelGetProcessTimeCounter`) stand still, for every guest thread. `sceKernelReadTsc`, the wall clocks and timed waits keep real time. Shader recompilation at first use and graphics pipeline builds are not covered. A console builds no pipelines, so whether a title that times its GPU work with a deadline also sees the pause in its audio or frame pacing has to be checked per title.
