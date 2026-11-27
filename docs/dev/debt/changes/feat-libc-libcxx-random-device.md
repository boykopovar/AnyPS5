# libc

## add Functional

- [std::__1::random_device](../../../core/libs/prx/libc/src/Random.cpp) (libc) - implements the three libc++ ABI imports listed for Azahar in RetroArch (PPSA99169, #5927). [LLVM libc++ on FreeBSD](https://github.com/llvm/llvm-project/blob/main/libcxx/src/random.cpp) uses stateless construction and destruction with a 32-bit system-entropy draw; this uses the existing cross-platform `HostEntropy_nid_no_patch` helper for the draw. The console firmware provider, token handling and entropy source have not been measured.
