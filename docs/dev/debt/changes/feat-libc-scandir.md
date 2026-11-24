# libc

## add Functional

- [scandir and alphasort](../../../core/libs/prx/libc/src/Directory.cpp) (libc) - follow POSIX.1-2008 over the guest's `readdir` entries: each kept entry is copied to the guest heap, the list is sorted with the caller's comparison, and failures leave the caller's list unset. The console's directory order and the guest `dirent` layout beyond the name are unverified.
