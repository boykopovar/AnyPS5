# libSceRandom technical debt

### Functional

- [sceRandomGetRandomNumber](../../../core/libs/prx/libSceRandom/Export.cpp) (libSceRandom) - a null buffer returns `SCE_RANDOM_ERROR_INVALID` (0x817C0016) even when the size is 0; that is an assumption, not measured on hardware. shadPS4 (PS4, `src/core/libraries/random/random.cpp`, commit 0614ce324be092791b758d79f93d9f1ca5b10a81) does not check the buffer, so it returns success for a null buffer of size 0. fpPS4 (PS4, `src/ps4_libscerandom.pas`, commit 04cefd43e6fddd1ab033e7980cd356d14c964905) rejects it, with `EINVAL` (22) where this library returns the SCE code. Firmware 13.42 checks only for null and stores directly (read from disassembly, not measured on a console), so the console faults on an unwritable non-null pointer; AnyPS5 returns INVALID (0x817C0016) instead.
