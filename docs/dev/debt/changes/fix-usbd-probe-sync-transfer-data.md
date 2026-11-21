# libSceUsbd

## add Functional

- [sceUsbdBulkTransfer, sceUsbdInterruptTransfer](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for `data` the host cannot read, or cannot write for an IN endpoint, is a host-safety fallback with console behavior unmeasured. Before the handle is looked up, OUT data is read in 4 KiB chunks through `GuestCopyIn_nid_no_patch` and IN data is checked with `GuestWritable_nid_no_patch`. The transfer runs on unfilled host storage of `length` plus one byte, which libusb's Windows HID backend can write for a report ID; a failed allocation returns `LIBUSB_ERROR_NO_MEM` as a guest error. For an IN endpoint only the bytes libusb reports, capped at `length` and including a partial count on timeout, are stored through `GuestCopyOut_nid_no_patch`.
