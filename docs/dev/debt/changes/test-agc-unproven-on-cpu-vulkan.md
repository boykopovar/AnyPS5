# libSceAgcDriver

## add Functional

- These agc tests skip on a CPU Vulkan device (llvmpipe, lavapipe) on Linux and stay unproven on it. Each failed on every run of the Linux main build, whose runner has only llvmpipe, and passes on the real GPUs the team runs. Nothing here affects a player, since no title runs on a CPU device. Every skip prints `unproven on CPU Vulkan (llvmpipe)` and the signature it saw, and real GPUs still run all of them.
  - [`agc_driver_global_atomic_csub`](../../../core/libs/prx/libSceAgcDriver/tests/execution/GlobalAtomicCsub.cpp): `global atomic csub without glc: wave64 byte 644 is 146, expected 0`, a wave64 shader on a subgroup size of 8.
  - [`agc_driver_coherent_atomic_loads_enabled`](../../../core/libs/prx/libSceAgcDriver/tests/execution/CoherentAtomicLoads.cpp): the waiting dispatch never finishes (`recorded batch still running on the GPU after 10 s`, test timeout).
  - [`agc_driver_host_write_shadowed_unit`](../../../core/libs/prx/libSceAgcDriver/tests/execution/HostWriteShadowedUnit.cpp): `the import made without the GPU lock was refused`.
  - [`agc_draw_input_reuse`, `agc_draw_input_mirrors` and `agc_driver_recorder`](../../../core/libs/prx/libSceAgcDriver/tests/Recorder.cpp): `an unchanged draw input was copied again` (draw snapshot reuse and draw input reuse) and `a reused draw input came back without its device-local mirror` (mirrors on only). These use compared host imports like the hook clear access tests, so the same cause is likely but not shown.
- `agc_driver_scattered_runs` and `agc_driver_image_table_packed_guards` also fail on llvmpipe, but they passed on some runs of the same main build. They are not skipped, so a race is not hidden.
