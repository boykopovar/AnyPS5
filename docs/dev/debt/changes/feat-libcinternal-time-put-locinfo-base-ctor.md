# libSceLibcInternal

## remove [_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom]

## add Additional callable exports

- [_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `IuFImJ5+kTk`) - not checked against `libSceLibcInternal.sprx` or on a console, and no title was seen calling it. `time_put` has no virtual bases, so under the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) this base-object constructor does the work of the complete-object `_Locinfo` constructor in `TimeGetFields.cpp` and runs it, with its limits: the C-locale `TimeInfo` is copied whatever the `_Locinfo`, and a null facet or `_Locinfo` throws `std::invalid_argument`
