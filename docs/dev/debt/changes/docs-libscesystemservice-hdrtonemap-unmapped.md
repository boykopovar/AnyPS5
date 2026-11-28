# libSceSystemService

## change [sceSystemServiceGetHdrToneMapLuminance guest output]

- [sceSystemServiceGetHdrToneMapLuminance guest output](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - returning SYSTEM_SERVICE_ERROR_PARAMETER for an unwritable output pointer is a host-safety fallback. A PS5 (firmware 11.60, payload process; a title process may differ) answered 0 for a valid buffer and wrote 12 bytes, and answered `0x80a10003` for a NULL output pointer, as AnyPS5 does; for an unmapped pointer (0x10) the call ended in a segmentation fault (signal 11, one run, prototype assumed), and a read-only buffer is unmeasured.
