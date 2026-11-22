# libSceLibcInternal

## add Functional

- [Windows posix_spawn open and closed standard streams](../../../core/libs/prx/libSceLibcInternal/Spawn.cpp) (libSceLibcInternal): on Windows, `open` file actions delegate to `open` and `close` (throwing `NotImplemented` for `app0.zar` archived files or virtual descriptors without a Windows handle), and closing all three standard streams `0` to `2` detaches the parent handles with `STARTF_USESTDHANDLES` so `libc` binds them to `NUL` rather than inheriting parent handles; invalid open flags return `EINVAL` (`22`) on both Windows and POSIX. Console equivalence is unverified.
