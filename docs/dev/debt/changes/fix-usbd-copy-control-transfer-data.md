# libSceUsbd

## add Functional

- [sceUsbdControlTransfer, sceUsbdGetDescriptor](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the guest `data` is read into a host copy through `GuestCopyIn_nid_no_patch`, and for a device-to-host request written back through `GuestCopyOut_nid_no_patch` before the handle is looked up; libusb transfers into the host copy and the transferred bytes are stored through `GuestCopyOut_nid_no_patch`. Returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for `data` the host cannot read, or cannot write for a device-to-host request, is a host-safety fallback with console behavior unmeasured.
