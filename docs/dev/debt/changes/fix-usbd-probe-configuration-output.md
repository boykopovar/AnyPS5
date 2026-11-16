# libSceUsbd

## add Functional

- [sceUsbdGetConfiguration](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the `configuration` output is cleared through `GuestCopyOut_nid_no_patch` before the handle is looked up and before libusb is queried, so an unwritable output returns `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) with `errno` unchanged; that result is a host-safety fallback with console behavior unmeasured, and a failed query now leaves 0 in the output.
