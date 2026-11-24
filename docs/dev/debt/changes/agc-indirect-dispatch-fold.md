# libSceAgcDriver

## add Functional

- [Indirect workgroup count fold](../../../core/libs/prx/libSceAgcDriver/Graphics/shaders/DispatchArguments.comp) (libSceAgcDriver) - a GPU-side `DISPATCH_INDIRECT` over `maxComputeWorkGroupCount` runs no workgroup, where the console runs them all, when its T workgroups exceed `maxComputeWorkGroupCount[0]` or 2^31 - 1, when it has 2^32 invocations or more (0xffffffff groups thus run nothing), or when its program is a split table call part. Otherwise it runs as one `T x 1 x 1` dispatch whose program rebuilds its workgroup ids from the X and Y counts the driver copies into ShaderData, which therefore stays out of push constants; that cost is not measured. Its workgroups start in linear order, not in X, Y, Z order; whether a title depends on that is not measured. Counts read on the CPU run as the chunks of the [Workgroup count split](../../../core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp).
