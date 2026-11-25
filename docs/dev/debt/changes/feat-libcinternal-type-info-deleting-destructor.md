# libSceLibcInternal

## add Functional

- [_ZNSt9type_infoD0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal, NID `N5nZ3wQw+Vc`) only frees the object through `libc` `operator delete(void*)`: under the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) `D0` is `D1` followed by `operator delete`, and `std::type_info`'s `D1` is the bare `ret` noted under Empty facet and exception members, as for the `__cxxabiv1` type_info `D0` exports in TypeInfoDestructors.cpp. The console's `D0` body was not read, so whether it frees through the same heap or calls the sized `operator delete` is unverified, and no title was seen calling it
