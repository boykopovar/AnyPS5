# libc

## add Unknown function info

- [_ZSt22_Random_device_entropyv](../../../core/libs/prx/libc/src/Random.cpp) (libc) - returns 32.0, the entropy of one 32-bit `random_device` result as this library's `operator()` produces from host entropy; follows the documented `random_device::entropy` behaviour (MicrosoftDocs `random-device-class`, cppreference `random_device::entropy`) and not a copied implementation. Not measured on a console. Closes when: measured on a console.

# libSceGnmDriver

## add Unknown function info

- [Validate queries](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the nine validation entry points answer as the Neo provider does (validation not enabled; version and on-submit checks 0), which the console's regular module must also export: Heretic + Hexen (CUSA51664) imports all nine from `libSceGnmDriver` while AnyPS5 only provided them in `libSceGnmDriverForNeoMode`. Not measured on a console. Complements #27342 (shared compute waits and GPU queries). Closes when: measured on a console.
