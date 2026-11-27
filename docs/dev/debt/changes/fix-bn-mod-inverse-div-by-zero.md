# libSceLibreSsl

## change [BN_mod_inverse]

- [BN_mod_inverse](../../../core/libs/prx/libSceLibreSsl/src/Bignum.cpp) error publication is unverified on PS5. Non-coprime operands with a nonzero modulus of either sign (upstream works modulo |n|) queue `BN_R_NO_INVERSE` (3/0xfff/108) and a zero modulus queues `BN_R_DIV_BY_ZERO` (3/0xfff/103), following LibreSSL 2.5.4 [`bn_gcd.c` lines 294-298, 519-540 and 701-722](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/bn/bn_gcd.c#L519-L540), [`bn_div.c` lines 148-151](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/bn/bn_div.c#L148-L151) and [`err.h` lines 160, 241](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/err/err.h#L241). Other BIGNUM error paths still lack publication. Constant-time flags do not select a separate algorithm.
