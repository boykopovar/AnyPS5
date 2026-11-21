# libSceUsbd

## add Functional

- [sceUsbdGetActiveConfigDescriptor, sceUsbdGetConfigDescriptor, sceUsbdGetConfigDescriptorByValue](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the `config` output is cleared through `GuestCopyOut_nid_no_patch` before the device is looked up and the descriptor is stored through it after libusb returns one (freeing the descriptor if that store fails); returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for an unwritable non-null `config` is a host-safety fallback with console behavior unmeasured.
