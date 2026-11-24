# libSceLibcInternal

## add Functional

- [posix_spawn open and closed standard streams](../../../core/libs/prx/libSceLibcInternal/Spawn.cpp) (libSceLibcInternal): on Windows, `open` file actions delegate to `open` and `close` (throwing `NotImplemented` for `app0.zar` archived files or virtual descriptors without a Windows handle), and closing all three standard streams `0` to `2` detaches the parent handles with `STARTF_USESTDHANDLES` so `libc` binds them to `NUL` rather than inheriting parent handles; `posix_spawn` and `posix_spawnp` preserve `errno` and `__error`, and invalid open flags and `O_DIRECTORY` return `EINVAL` (`22`) and `ENOTDIR` (`20`) on both Windows and POSIX. Console equivalence is unverified.
