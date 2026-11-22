# libSceLibcInternal

## remove `bxLH5WHgMBY`

## add Functional

- [std::ios_base::Init base-object destructor](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal, NID `bxLH5WHgMBY`) - the firmware body and console execution are unverified. The base-object destructor shares the complete-object destructor's counter and final flush of libc's `_Stdout` and `_Stderr`; this library has no separate narrow standard stream objects. `Init` has no virtual bases in Dinkumware's [xiosbase](https://github.com/microsoft/STL/blob/54d74d7a58e828857a73f979eccc2ba1c02e256d/stl/inc/xiosbase), so the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) gives both forms the same teardown. The existing complete-object implementation follows [iostream.cpp](https://github.com/microsoft/STL/blob/54d74d7a58e828857a73f979eccc2ba1c02e256d/stl/src/iostream.cpp#L9-L36) and the CUSA00900 `libc.prx` disassembly documented in [PR #21832](https://github.com/boykopovar/AnyPS5/pull/21832).
