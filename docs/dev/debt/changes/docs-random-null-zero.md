# libSceRandom

## change [sceRandomGetRandomNumber]

- [sceRandomGetRandomNumber](../../../core/libs/prx/libSceRandom/Export.cpp) (libSceRandom) - in a PS5 payload process on firmware 13.40, a null buffer with size 0 returned `SCE_RANDOM_ERROR_INVALID` (0x817C0016). A writable buffer returned success with sizes 0 and 1, and the zero-size call left its byte unchanged. AnyPS5 matches all three cases. Firmware 13.42 checks only for null and stores directly (read from disassembly, not measured on a console), so the console faults on an unwritable non-null pointer; AnyPS5 returns INVALID (0x817C0016) instead.
