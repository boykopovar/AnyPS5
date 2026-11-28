# libSceLibcInternal

## add Additional callable exports

- [time_get<char> D0 and D1 and time_get<wchar_t> D1](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal, NIDs `mmq9OwwYx74`, `Cp9ksNOeun8` and `QTgRx1NTp6o`) - not checked against `libSceLibcInternal.sprx` or on a console, and no title was seen calling them. `time_get` has no virtual bases, so under the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) each `D1` runs that facet's `D2` export in `TimeGetFields.cpp`, and the `char` `D0` runs `D2` and then `operator delete` (`_ZdlPv`), as the wide `D0` there does; a null facet throws `std::invalid_argument` through `D2`
