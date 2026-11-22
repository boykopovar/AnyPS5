# libSceIcu

## add Functional

- [Data swappers](../../../core/libs/prx/libSceIcu/DataSwapper.cpp) follow ICU 68.2 [udataswp.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/udataswp.cpp) and [uinvchar.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uinvchar.cpp) and are not measured on a console. The eight `udata_*` swapper exports take the signatures of the internal `udataswp.h`; the 96-byte `UDataSwapper` layout is unverified against the firmware. `UBool` flags are compared as ICU 68's `int8_t` raw values, not as the `bool` of the bundled 78.1. A null swapper in `udata_readInt16`, `udata_readInt32` and `udata_printError` throws `std::invalid_argument` where ICU dereferences it. Closes when: compared with the console.
