# libc

## change fail on macOS with `ENAMETOOLONG` once their host form

- [Guest path names](../../../core/libs/prx/libc/src/General.cpp) (libc) whose host form under the root directory reaches the host `PATH_MAX` are passed to the host relative to the root, which is the host working directory. On macOS a guest path that resolves to 1024 bytes or more relative to the root still fails with `ENAMETOOLONG`: a relative name used from a deep working directory, a path under an alias such as `/app0`, which stays absolute, and a path joined to a directory descriptor's host path by the `*at` calls. `chdir` into such a directory fails with `ENAMETOOLONG` when a component past the host limit is a symbolic link, because the root check then cannot resolve it.
