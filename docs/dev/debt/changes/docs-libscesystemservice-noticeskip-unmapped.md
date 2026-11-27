# libSceSystemService

## change [sceSystemServiceGetNoticeScreenSkipFlag guest output]

- [sceSystemServiceGetNoticeScreenSkipFlag guest output](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - returning SYSTEM_SERVICE_ERROR_PARAMETER for an unwritable output pointer is a host-safety fallback. A PS5 (firmware 11.60, payload process without an app status; a title process may differ) answered `0x80a10002` for a valid buffer and left it unchanged, and answered `0x80a10003` for a NULL output pointer, as AnyPS5 does; the one run with an unmapped pointer (0x10) ended without a result and without the probe's end line, so what the console returns for an unwritable pointer is unmeasured (prototype assumed).
