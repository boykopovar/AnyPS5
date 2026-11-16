# libSceUsbd

## add Functional

- [sceUsbdBulkTransfer, sceUsbdInterruptTransfer](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - before the handle is looked up, `data` is read through `GuestCopyIn_nid_no_patch` and, for an IN endpoint, written back through `GuestCopyOut_nid_no_patch`; returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for data the host cannot read, or cannot write for an IN endpoint, is a host-safety fallback with console behavior unmeasured. Without it the call reaches libusb, which reports a host I/O error and leaves the host `errno` behind.
