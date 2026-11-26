# libSceLibcInternal

## remove `-Bl9-SZ2noc`

## remove `sqWytnhYdEg`

## remove `kxXCvcat1cM`

## remove `Uw3OTZFPNt4`

## remove `57mMrw0l-40`

## add Functional

- [std::ios_base::Init and std::_Winit constructors and destructors](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal, NIDs `sqWytnhYdEg`, `kxXCvcat1cM`, `-Bl9-SZ2noc`, `Uw3OTZFPNt4`, `57mMrw0l-40`, `bTQcNwRc8hE`) - the console constructs the library's narrow or wide standard streams on first use and flushes them on final destruction; this library has no equivalent streams, so construction only updates a counter and final destruction flushes libc's `_Stdout` and `_Stderr`. Not verified against `libSceLibcInternal.sprx` or on a console. Counter behaviour follows Dinkumware's [iostream.cpp](https://github.com/microsoft/STL/blob/54d74d7a58e828857a73f979eccc2ba1c02e256d/stl/src/iostream.cpp#L9-L36), [wiostrea.cpp](https://github.com/microsoft/STL/blob/54d74d7a58e828857a73f979eccc2ba1c02e256d/stl/src/wiostrea.cpp#L9-L33), and the CUSA00900 `libc.prx` disassembly. C2 constructors forward to C1; the wide D2 alias shares the D1 release operation.
