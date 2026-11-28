# libSceLibcInternal

## remove qEob3o53s2M

## remove xFva4yxsVW8

## remove Lf6h5krl2fA

## add Functional

- [numpunct<char> destructors](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NIDs `qEob3o53s2M`, `xFva4yxsVW8`, `Lf6h5krl2fA`) run libc's bodies for the same NIDs, so the limits of the libc `numpunct<char>` destructor entry apply: they run `_Tidy`, the deleting one then `operator delete`, and a null facet throws `std::invalid_argument` where the console faults. Neither `libSceLibcInternal.sprx` body was read, so whether it differs from the `libc.prx` one that entry follows is not known
