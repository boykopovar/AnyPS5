# libSceLibcInternal

## remove _ZNSt7collateIcEC2ERKSt8_Locinfom

## remove _ZNSt7collateIcEC2Em

## remove _ZNSt7collateIcED0Ev

## add Functional

- [The remaining collate<char> constructor forms and its deleting destructor](../../../core/libs/prx/libSceLibcInternal/Collate.cpp) (libSceLibcInternal, NIDs `L71JAnoQees`, `tkqNipin1EI`, `Lt4407UMs2o`): the base-object `(const _Locinfo&, size_t)` and `(size_t)` constructors do what their implemented complete-object forms do, as the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling-special-ctor-dtor) emits identical complete- and base-object constructors for a class without virtual bases, and share their unverified points. The deleting destructor runs the complete destructor (nothing to release, as `D1`/`D2` here) and frees the object with `operator delete`, as the facet vtable's delete entry in libc does. The console's bodies for these NIDs were not read. A null object throws.
