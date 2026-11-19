# libc

## add Functional

- [_Dclass, _FDclass](../../../core/libs/prx/libc/src/MathExact.cpp) and [__fpclassifyd](../../../core/libs/prx/libc/include/SonyCrt.hpp) (libc, also libSceAmpr) classify binary64 and binary32 by integer exponent and fraction fields without changing errno or floating-point exception flags, including signaling NaNs and subnormals, returning the Dinkumware codes of [xdtest.c](https://github.com/ganboing/crt.vc12/blob/f1d47126bf13d1c9dccaa6c53b72217fd0474968/src/xdtest.c) and [xfdtest.c](https://github.com/ganboing/crt.vc12/blob/f1d47126bf13d1c9dccaa6c53b72217fd0474968/src/xfdtest.c). Console exception-flag behaviour is unmeasured.
