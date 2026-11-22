# libSceLibcInternal

## add Functional

- [num_put<wchar_t> complete-object constructor](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal, NID `Ky+C-qbKcX0`) - `num_put<wchar_t>::num_put(const _Locinfo&, size_t)` `C1` runs the `C2` export in NumPutField.cpp with the same arguments: under the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) the two do the same work for a class without virtual bases, so the limits of the `num_put<wchar_t>` constructor entry apply. The console's `C1` body was not read and no title was seen calling it
