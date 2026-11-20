# libSceUsbd

## add Functional

- [sceUsbdGetConfiguration](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the `configuration` output is checked with `GuestWritable_nid_no_patch` before the handle is looked up and before libusb is queried, so an unwritable output returns `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) with `errno` unchanged, and it is written only after a successful query; that result is a host-safety fallback with console behavior unmeasured.
