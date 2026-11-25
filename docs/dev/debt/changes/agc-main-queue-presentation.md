# libSceAgcDriver

## add Functional

- [Separate presentation queue on Windows Pascal](../../../core/libs/prx/libSceAgcDriver/Execution/src/VulkanDevice.cpp) (libSceAgcDriver) - FIFO presentation with a separate queue loses the device on a GTX 1060 3GB with NVIDIA 581.80 on Windows 11 10.0.26300 ([#19798](https://github.com/boykopovar/AnyPS5/issues/19798)). `APS5_NO_PRESENT_QUEUE` uses the existing main-queue path under the GPU mutex; the requested queue plan is unchanged. This avoids the reproduced loss but can hold up draw work during the present. The driver or synchronization cause and GTX 1070 confirmation remain unresolved. Closes when: the affected modes pass with the separate queue on the affected hardware.
