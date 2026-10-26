# libs technical debt

### Unknown function info

- [SceAvPlayerVideoEx](../../../core/libs/SceTypes.hpp) (libSceAvPlayer) - frame rate field and encoding unknown; it is left zero in frame and stream info

### Functional

- [GuestKernelSys.cpp](../../../core/libs/tests/GuestKernelSys.cpp) (libs) - Windows removes the fixture's symbolic links and the fixture without std::filesystem: the MinGW libstdc++ (GCC 15.2.0) reports a dangling symbolic link as missing and clears the error, so `remove` neither deletes it nor fails, and `remove_all` loops forever over a tree that holds one (`recursive_directory_iterator::__erase` retries the entry while the error stays clear). The tests delete reparse points with `DeleteFileW` and `RemoveDirectoryW` and the fixture through the shell
