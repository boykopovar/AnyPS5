# libc

## add Functional

- [std::__1::__next_prime](../../../core/libs/prx/libc/src/LibcxxNextPrime.cpp) (libc) - a named libc++ ABI import of converted cores including Azahar in RetroArch (PPSA99169, #5927), not verified as a firmware export. It returns zero for zero and the smallest prime at least as large as every other input, throwing guest `std::overflow_error` above the largest 64-bit prime, following [LLVM libc++](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/hash.cpp#L31-L68). It uses deterministic Miller-Rabin with the first twelve prime bases under the [Sorenson and Webster bound](https://arxiv.org/abs/1509.00864). Exception matching against the compiler runtime bundled by RetroArch is unverified.
