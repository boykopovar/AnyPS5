# libkernel

## add Functional

- [dlopen flags](../../../core/libs/prx/libkernel/Module/src/DynamicLoader.cpp) (libkernel) - flag bits other than the mode, `RTLD_GLOBAL` (0x100), `RTLD_NODELETE` (0x1000) and `RTLD_NOLOAD` (0x2000) return NULL with a `dlerror` message: `RTLD_TRACE` (0x200), and unknown bits that FreeBSD 11 rtld ignores. `RTLD_NODELETE` and `RTLD_NOLOAD` follow FreeBSD 11 rtld, except that on Windows `RTLD_NOLOAD` finds a loaded image only by its full path or base name, not by file identity; whether the console's dlopen, a wrapper over Sony's dynlib calls, honours them the same way is not measured.
