# libSceLibcInternal

## remove [_ZNSt7collateIcEC1Em]

## remove [_ZNSt7collateIcEC1EPKcm]

## remove [_ZNSt7collateIcEC1ERKSt8_Locinfom]

## remove [_ZNSt7collateIcEC2EPKcm]

## add Functional

- [collate<char> constructors](../../../core/libs/prx/libSceLibcInternal/Collate.cpp) (libSceLibcInternal, NIDs `Oo1r8jKGZQ4`, `splBMMcF3yk`, `raLgIUi3xmk` and `VClCrMDyydE`): the console's `libSceLibcInternal.sprx` was not read and no title was seen calling these constructors. Following [locale](https://github.com/microsoft/STL/blob/165a67afdde92bf3e5248be35b6f9240b3d1efb5/stl/inc/locale#L93-L124) (`crt.vc12` [locale](https://github.com/ganboing/crt.vc12/blob/f1d47126bf13d1c9dccaa6c53b72217fd0474968/src/locale#L49-L90)), `C1(size_t)` and `C1`/`C2(const char*, size_t)` validate the locale via libc's `_Locinfo` accepting only `"C"`, `C1(const _Locinfo&, size_t)` borrows `_Locinfo`, null `self`/`_Locinfo` throw `std::invalid_argument`, and all four install `_ZTVSt7collateIcE` (whose `_Incref`/`_Decref` count low 32 bits), write 64-bit `references`, and zero pointers at offsets `0x10` and `0x18`; non-C locales remain unsupported.
