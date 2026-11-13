# libSceJpegEnc technical debt

### Functional

- [libSceJpegEnc](../../../core/libs/prx/libSceJpegEnc/Export.cpp) returns SCE_JPEG_ENC_ERROR_INVALID_ADDR for unmapped or unwritable pointer arguments and buffers and SCE_JPEG_ENC_ERROR_INVALID_HANDLE for an unreadable handle. This is a defensive host-protection fallback; console behaviour for unmapped or read-only memory is unmeasured.

### Unimplemented functionality

- [sceJpegEncEncode](../../../core/libs/prx/libSceJpegEnc/Export.cpp) (libSceJpegEnc) - MJPEG mode throws; its encoding path is not implemented. Restart intervals are confirmed by the native validator, MCU counter and DRI writer: 0 disables restart markers, -1 selects one MCU row, and 1 through 65535 specify an MCU count. Other values return the invalid-parameter error. A successful call returns the JPEG's size, as a PS4 Pro (firmware 10.71) does; this is not confirmed on a PS5; the size itself comes from this encoder, so it differs from the console's for the same image (the console 633 bytes, this encoder 650, for an 8x8 RGBA image at 4:2:0).
