# libSceUsbd

## add Functional

- [sceUsbdBulkTransfer and sceUsbdInterruptTransfer](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the `transferred` count is cleared through `GuestCopyOut_nid_no_patch` before the handle is looked up, libusb counts into a host local, and the result is copied back the same way; returning `SCE_USBD_ERROR_INVALID_ARG` for an unwritable non-null `transferred` is a host-safety fallback (a null one still throws), and whether the console faults or returns an argument error on unmapped memory is unmeasured. The `data` buffer is still handed to libusb in place.
