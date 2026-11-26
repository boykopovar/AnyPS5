# libSceLibcInternal

## change [_Stat and _Lstat without an error output, _File_size(const char*), _Last_write_time(const char*), _Set_last_write_time, _Rename, _Resize, _Copy_file, _Statvfs and _Current_set2]

- [_Stat and _Lstat without an error output, _File_size(const char*), _Last_write_time(const char*), _Set_last_write_time, _Rename, _Resize, _Copy_file, _Statvfs and _Current_set2](../../../core/libs/prx/libSceLibcInternal/UnversionedFilesystemCalls.cpp) (libSceLibcInternal) - names and NIDs come from the PS5 3.20 export inventory; no module was read and no console measured. The overloads without an error output call this library's overloads with one, passing no output, and `_Last_write_time(const char*)` returns -1 when that call fails and the seconds otherwise. The other six call the matching `std::__sce_v2::filesystem` helpers of libc, which follow the `libc.prx` bundled with PPSA03671 (see [libc](libc.md)), on the assumption that both namespaces build the same Dinkumware sources; so `_Statvfs` returns `ENOSYS` with unknown space and `_Current_set2` accepts only "/". `_Equivalent` and the three-argument `_Xfilesystem_error` still throw

## change [_ZNSt10filesystem12_Current_setEPKc]

- [_ZNSt10filesystem12_Current_setEPKc](../../../core/libs/prx/libSceLibcInternal/UnversionedFilesystemCalls.cpp) (libSceLibcInternal, NID `Yl10kSufa5k`) - takes a narrow path from the mangled name and returns a boolean as in the wide-path [Dinkumware implementation](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/src/filesys.cpp#L151-L159). It reports success when the existing guest `_Current_set2` succeeds, so only the virtual root `/` succeeds and the host working directory is untouched. The return type and behavior are unverified on PS5.
