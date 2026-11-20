# libSceLibcInternal

## remove `2yOarodWACE`

## add Functional

- [_ZNSt6_WinitD2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal, NID `2yOarodWACE`) - not checked against `libSceLibcInternal.sprx` or on a console, `_ZNSt6_WinitC2Ev` (`57mMrw0l-40`) still throws, and this library has no separate wide standard stream objects so final release flushes libc's `_Stdout` and `_Stderr` only. Shares `wideStreamInitCount` and `ReleaseStandardStreamInit` with `_ZNSt6_WinitD1Ev` (decrementing from -1 and flushing at 0), matching Dinkumware's [wiostrea.cpp](https://github.com/microsoft/STL/blob/165a67afdde92bf3e5248be35b6f9240b3d1efb5/stl/src/wiostrea.cpp) and [iostream](https://github.com/microsoft/STL/blob/165a67afdde92bf3e5248be35b6f9240b3d1efb5/stl/inc/iostream) lifetime semantics; under the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) base-object `D2` performs the same teardown as `D1` for a class without virtual bases.
