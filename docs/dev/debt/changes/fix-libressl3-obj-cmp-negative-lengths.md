# libSceLibreSsl3

## change [OBJ_cmp]

- [OBJ_cmp](../../../core/libs/prx/libSceLibreSsl3/src/ObjCmp.cpp) (libSceLibreSsl3) - unequal lengths now return their difference before any sign check, as [`obj_lib.c`](https://github.com/libressl/openbsd/blob/bc9483725c5815efb64b6d9acc52fc5fba139212/src/lib/libcrypto/objects/obj_lib.c) subtracts them before reading data, so an unequal negative length no longer throws. The difference is computed in 64 bits and throws `std::overflow_error` when it does not fit in `int`, where LibreSSL's subtraction is undefined. Only equal negative lengths, which reach LibreSSL's `memcmp`, throw `std::invalid_argument`. The object and its data are still read directly; PS5 behaviour is unverified. Closes when: compared with the console.
