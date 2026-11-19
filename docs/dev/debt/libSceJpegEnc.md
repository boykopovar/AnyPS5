# libSceJpegEnc technical debt

### Functional

- [libSceJpegEnc](../../../core/libs/prx/libSceJpegEnc/Export.cpp) returns SCE_JPEG_ENC_ERROR_INVALID_ADDR for unmapped or unwritable pointer arguments and buffers and SCE_JPEG_ENC_ERROR_INVALID_HANDLE for an unreadable handle. This is a defensive host-protection fallback; console behaviour for unmapped or read-only memory is unmeasured.

### Unimplemented functionality

- [sceJpegEncEncode](../../../core/libs/prx/libSceJpegEnc/Export.cpp) (libSceJpegEnc) - MJPEG mode throws; its encoding path is not implemented. Restart intervals are confirmed by the native validator, MCU counter and DRI writer: 0 disables restart markers, -1 selects one MCU row, and 1 through 65535 specify an MCU count. Other values return the invalid-parameter error. A successful call returns the JPEG's size, as a PS4 Pro (firmware 10.71) and a PS5 payload process (firmware 11.60) do; the size itself comes from this encoder, so it differs from the console's for the same image (the console 633 bytes, this encoder 650, for an 8x8 RGBA image at 4:2:0). For four colour bars at 8x8, 4:2:0, the payload returned 694 bytes at compression ratio 0, 656 at 128, 634 at 255 and 700 with restart interval 1 or -1; this encoder gives 685, 657, 640 and 691; a title process was not measured.
