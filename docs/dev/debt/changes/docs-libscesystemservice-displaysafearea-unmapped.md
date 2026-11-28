# libSceSystemService

## change [sceSystemServiceGetDisplaySafeAreaInfo guest output]

- [sceSystemServiceGetDisplaySafeAreaInfo guest output](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - returning SYSTEM_SERVICE_ERROR_PARAMETER for an unwritable output pointer is a host-safety fallback. A PS5 (firmware 11.60, payload process; a title process may differ) answered 0 for a valid 132-byte buffer and changed only its first 4 bytes, to the float 1.0 (the same in a second call); for an unmapped pointer (0x10) the call ended in a segmentation fault (signal 11, one run). Prototype and buffer layout (a float and 128 reserved bytes) assumed.
