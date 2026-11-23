# libSceLibcInternal

## remove `6iDi6e2e4x8`

## remove `0Pd-K5jGcgM`

## add Functional

- [num_put<wchar_t> constructor and _Getcat](../../../core/libs/prx/libSceLibcInternal/NumPutField.cpp) (libSceLibcInternal, NIDs `6iDi6e2e4x8`, `0Pd-K5jGcgM`) - `num_put<wchar_t>::_Getcat` and `num_put<wchar_t>::num_put(const _Locinfo&, size_t)` were not checked against `libSceLibcInternal.sprx` or on a console. They populate the facet with a non-owning reference count matching Dinkumware's `locale::facet` and install a local vtable with real `destroy`/`delete`/`retain`/`release` slots, while all eight `do_put` virtual slots still forward to the existing throwing exports.
