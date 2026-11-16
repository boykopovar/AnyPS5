# libSceUsbd

## add Functional

- [sceUsbdBulkTransfer, sceUsbdInterruptTransfer](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - before the handle is looked up, `data` for an OUT endpoint is read through `GuestCopyIn_nid_no_patch` in 4 KiB chunks and `data` for an IN endpoint is checked with `GuestWritable_nid_no_patch`, so no host buffer is sized by an unchecked guest `length`; the transfer then runs on host storage, OUT data is copied in first, and for an IN endpoint only the bytes libusb reports, including a partial count on timeout, are stored through `GuestCopyOut_nid_no_patch`; returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for data the host cannot read, or cannot write for an IN endpoint, is a host-safety fallback with console behavior unmeasured. Without it the call reaches libusb, which reports a host I/O error and leaves the host `errno` behind.
