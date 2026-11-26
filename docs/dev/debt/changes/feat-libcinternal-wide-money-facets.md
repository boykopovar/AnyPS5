# libSceLibcInternal

## remove `x5yAFCJRz8I`

## remove `-hrHhi-UFxs`

## remove `DTH1zTBrOO8`

## remove `aTjYlKCxPGo`

## add Functional

- [money_get<wchar_t> and money_put<wchar_t> construction and _Put](../../../core/libs/prx/libSceLibcInternal/MoneyGetPut.cpp) (libSceLibcInternal, NIDs `x5yAFCJRz8I`, `DTH1zTBrOO8`, `aTjYlKCxPGo`, `-hrHhi-UFxs`) - `money_get<wchar_t>::money_get(size_t)`, `money_get<wchar_t>::_Getcat`, `money_put<wchar_t>::money_put(const _Locinfo&, size_t)` and `money_put<wchar_t>::_Put` were not checked against `libSceLibcInternal.sprx` or on a console. The constructors populate the facet object with a non-owning reference count matching Dinkumware's `locale::facet` and install local vtables whose `do_get`/`do_put` slots still forward to the existing throwing exports; `_Put` is implemented as a static 2-byte element writer following Dinkumware's [xlocmon](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocmon).
