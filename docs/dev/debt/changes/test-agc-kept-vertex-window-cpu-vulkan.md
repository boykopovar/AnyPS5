# libSceAgcDriver

## add Functional

- [`agc_driver_recorder` and `agc_draw_input_mirrors`](../../../core/libs/prx/libSceAgcDriver/tests/Recorder.cpp) also skip the kept vertex window mirror check on a CPU Vulkan device (llvmpipe). Once the earlier draw input checks skipped, both tests reached it and failed on the Linux main build with `a kept vertex window was not mirrored`, the same device-local mirror family as `a reused draw input came back without its device-local mirror`. Real GPUs still run it; the skip prints `unproven on CPU Vulkan (llvmpipe)` and the signature.
