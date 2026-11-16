# libSceUsbd

## add Functional

- [sceUsbdGetStringDescriptor, sceUsbdGetStringDescriptorAscii](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the first byte of `data` is cleared through `GuestCopyOut_nid_no_patch` before the handle is looked up, and the descriptor (or the ASCII string and its terminator) is built in a host buffer and copied out through it; returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for an unwritable non-null `data` is a host-safety fallback with console behavior unmeasured.
