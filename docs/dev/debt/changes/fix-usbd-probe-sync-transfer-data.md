# libSceUsbd

## add Functional

- [sceUsbdBulkTransfer, sceUsbdInterruptTransfer](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - before the handle is looked up, `data` for an OUT endpoint is read through `GuestCopyIn_nid_no_patch` in 4 KiB chunks and `data` for an IN endpoint is checked with `GuestWritable_nid_no_patch`, so no host buffer is sized by the guest `length`; returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for data the host cannot read, or cannot write for an IN endpoint, is a host-safety fallback with console behavior unmeasured. Without it the call reaches libusb, which reports a host I/O error and leaves the host `errno` behind.
