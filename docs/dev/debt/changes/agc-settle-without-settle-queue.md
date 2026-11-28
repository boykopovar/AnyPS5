# libSceAgcDriver

## add Functional

- [Host import watch decision outlives its device](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) (libSceAgcDriver) - `decideImportWatch` keeps its decision under the `VkDevice` handle and `ClearHostImports` does not drop it, so a device created at the handle of a destroyed one inherits that device's decision without a probe of its own. A context with no settle queue does not settle under an inherited decision; the inherited choice between watching and comparing, and the dma-buf selection on Linux, are still taken as they are. Seen on Windows with AMD 26.11.2, where the bare device of `agc_driver_native_multisample_depth` got the handle of the last driver device in 12 of 40 runs
