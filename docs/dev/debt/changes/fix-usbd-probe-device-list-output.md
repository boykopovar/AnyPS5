# libSceUsbd

## add Functional

- [sceUsbdGetDeviceList](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the `list` output is checked with `GuestWritable_nid_no_patch` before host devices are enumerated, so a failed enumeration leaves it untouched, and the list is stored through `GuestCopyOut_nid_no_patch` afterwards (freeing the host list and its references if that store fails); returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for an unwritable non-null `list` is a host-safety fallback with console behavior unmeasured.
- [sceUsbdGetDeviceDescriptor](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the descriptor is read into a host copy and stored through `GuestCopyOut_nid_no_patch`; returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for an unwritable non-null `descriptor` is a host-safety fallback with console behavior unmeasured.
