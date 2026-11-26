# libc

## add Unknown function info

- [_ZSt22_Random_device_entropyv](../../../core/libs/prx/libc/src/Random.cpp) (libc) - returns 32.0, the entropy of one 32-bit `random_device` result as this library's `operator()` produces from host entropy; follows the documented `random_device::entropy` behaviour (MicrosoftDocs `random-device-class`, cppreference `random_device::entropy`) and not a copied implementation. Not measured on a console. Closes when: measured on a console.
