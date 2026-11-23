# libSceHidControl

## add Functional

- [sceHidControlPswUpdateSendData and sceHidControlPswUpdateSendAppendixData](../../../core/libs/prx/libSceHidControl/Export.cpp) - The two update send exports read the eight-byte chunk size through `GuestCopyIn_nid_no_patch` before comparing it against the firmware-derived `0x7fd5` limit and forwarding the following bytes in an ioctl request. An unreadable non-null header returns the existing `0x803b3001` invalid-argument error without sending an ioctl, preserving the caller's `errno`. This is a host-safety fallback; the console's response to unreadable non-null guest memory is unmeasured. The bytes following a valid header are still passed through to the unimplemented HID device, whose request behavior remains unknown.
