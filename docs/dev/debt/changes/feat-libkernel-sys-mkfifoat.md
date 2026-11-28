# libkernel_sys

## add Functional

- [mkfifoat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - creates the FIFO for `AT_FDCWD` and absolute paths through `mkfifo`. A relative path with a directory descriptor throws `NotImplemented`, because this change does not resolve descriptor bases yet; `mkdirat` already does so.
