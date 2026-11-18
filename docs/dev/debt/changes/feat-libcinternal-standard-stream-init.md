# libSceLibcInternal

## remove `-Bl9-SZ2noc`

## remove `sqWytnhYdEg`

## remove `kxXCvcat1cM`

## remove `Uw3OTZFPNt4`

## add Functional

- [std::ios_base::Init and std::_Winit constructors and destructors](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal, NIDs `sqWytnhYdEg`, `kxXCvcat1cM`, `-Bl9-SZ2noc`, `Uw3OTZFPNt4`) - the console builds the library's own narrow or wide standard streams on the first construction and flushes them on the last destruction; this library has none, so construction only counts and the last destruction flushes libc's `_Stdout` and `_Stderr`. Not checked against `libSceLibcInternal.sprx` or on a console. The count (from -1, set to 1 when negative, flushed at 0) follows Dinkumware's [iostream.cpp](https://github.com/microsoft/STL/blob/54d74d7a58e828857a73f979eccc2ba1c02e256d/stl/src/iostream.cpp#L9-L36) and [wiostrea.cpp](https://github.com/microsoft/STL/blob/54d74d7a58e828857a73f979eccc2ba1c02e256d/stl/src/wiostrea.cpp#L9-L33) and the `libc.prx` bundled with CUSA00900 (disassembly, not run). The C2 and D2 aliases still throw
