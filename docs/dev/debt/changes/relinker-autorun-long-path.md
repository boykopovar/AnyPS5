# relinker

## add Functional

- A converted Windows executable can still fail with error 206 if its parent directory exceeds the Win32 current-directory length limit. The [entry stub](../../../core/relinker/elfpatcher/src/windows/WindowsEntryStubBuilder.cpp) calls `SetCurrentDirectoryW` before entering guest code. This was reproduced with a synthetic executable under an extended-length directory on Windows 11.
