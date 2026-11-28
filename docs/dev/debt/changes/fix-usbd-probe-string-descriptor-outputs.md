# libSceUsbd

## add Functional

- [sceUsbdGetStringDescriptor, sceUsbdGetStringDescriptorAscii](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the whole `length` of `data` (at most 65535 bytes) is checked as writable under a `GuestArena::HostWrite`, and its first byte cleared through `GuestCopyOut_nid_no_patch`, before the handle is looked up, and the descriptor (or the ASCII string and its terminator) is built in a host buffer and copied out through it; returning `SCE_USBD_ERROR_INVALID_ARG` (`0x80240002`) for a non-null `data` that is not writable for `length` bytes is a host-safety fallback with console behavior unmeasured.
