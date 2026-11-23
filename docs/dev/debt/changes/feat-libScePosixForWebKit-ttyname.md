# libScePosixForWebKit

## add Functional

- [ttyname and ttyname_r](../../../core/libs/prx/libScePosixForWebKit/Filesystem.cpp) (libScePosixForWebKit) - follow POSIX.1-2008: a descriptor that is not a terminal reports `ENOTTY`, a bad descriptor `EBADF`, and a name longer than the buffer `ERANGE`. A terminal's name comes from the host's `ttyname_r`. The console has no terminal for a guest, so its result for a terminal descriptor is unverified. Windows has no terminal names and throws `NotImplemented`.
