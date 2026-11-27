# libSceGnmDriver

## change [NeoMode-only imports]

- [NeoMode-only imports](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the 19 imports inFAMOUS Second Son asks of `libSceGnmDriver` that only `libSceGnmDriverForNeoMode` exported are now exported here too: `sceGnmSetEmbeddedPsShader` and `sceGnmSetEmbeddedVsShader` reuse the same embedded shader producers, `sceGnmSubmitCommandBuffersForWorkload`, `sceGnmSubmitAndFlipCommandBuffersForWorkload` and `sceGnmDingDongForWorkload` run as their versions without a workload (see Workload submissions), `sceGnmMapComputeQueueWithPriority` maps a queue as `sceGnmMapComputeQueue` does (see Compute queue priorities), and the other 13 throw `NotImplemented_nid_no_patch` until their console behavior is measured (#17581).

## add Functional

- [Workload submissions](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - not measured on a console. `sceGnmSubmitCommandBuffersForWorkload` and `sceGnmSubmitAndFlipCommandBuffersForWorkload` check and submit their buffers as `sceGnmSubmitCommandBuffers` and `sceGnmSubmitAndFlipCommandBuffers` do. Those checks are not the ones PS4 firmware 10.71 `libSceGnmDriverForNeoMode.sprx` makes (0xf80, 0x1410): that module skips them for a count of 0 and refuses a ccb size array without addresses whatever its sizes. `sceGnmDingDongForWorkload` returns 0 for queue 0, as that module does (0x4090), and otherwise rings the queue as `sceGnmDingDong`. The workload is not read: no stream or workload is recorded (see Workload streams), and the work runs as if submitted without one, as in [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/57e129ec548cfcf8572c0a26b1c9aaf7e95dc3b6/src/core/libraries/gnmdriver/gnmdriver.cpp).
