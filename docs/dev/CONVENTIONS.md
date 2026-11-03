# Coding Conventions

### C++ Naming Conventions

- Interface names (classes containing only pure virtual methods with no implementation) start with `I`, as in C#.
- Class names and public method names use PascalCase.
- Private method and field names use camelCase.
- Function argument names use camelCase.
- Template parameter names follow the C# style: `TKey`, `TValue`, `TIterator`.

---

Comments in code can only be added to indicate areas of [technical debt](TechnicalDebt.md) (only by a human), the end of `#endif`, and the end of namespace.

[Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/)

There's no need to add image files to the repository. You can add images to the [gist](https://gist.github.com/boykopovar/0e53f2e1426f29ecd41e3b51540b8a90) comments and paste a link into the md file.

### Host writes into guest memory

Host code that writes into guest memory through the kernel or a native library must hold a `GuestArena::HostWrite` ([GuestArena.hpp](../../core/libs/prx/libc/include/GuestArena.hpp)) over the destination range for the whole write. This covers file reads (`ReadFile`, `read`, `pread`), socket receives, `DeviceIoControl` and other calls that fill a caller's buffer.

The write watch can leave guest pages read-only between collections: every watched page on macOS, and on Windows the pages of shared views (shared memory, shared file mappings, and direct memory mapped at more than one guest address), which it tracks by write protection (see the write-watch and Windows direct-memory backing entries in [debt/libc.md](debt/libc.md)). A kernel-mode write into such a page doesn't fault into the watch. It fails instead, with `EFAULT` on macOS and `ERROR_NOACCESS` on Windows. `HostWrite` opens the pages and makes sure the write is recorded.

- A write that completes later on another thread (overlapped I/O, completion ports, asynchronous reads) is bracketed where the bytes land, not where the request was issued.
- Ordinary CPU stores from host C++ code need no bracket: the write watch records them (through a fault where the page is protected).
- A GPU import of guest memory uses `HostImportAccess`, not `HostWrite`.
- Third-party code that writes guest memory on its own (driver threads, library worker threads) can't be bracketed. List it in that library's debt file.
### Host-only test executables

A host-only test executable (one that links no guest library) whose sources inherit the `-fno-asynchronous-unwind-tables` policy of `core/libs/CMakeLists.txt`, and that is built for MinGW, must call `configure_host_test_executable(<target>)` (root `CMakeLists.txt`) after `add_test_executable`. It supplies the two flags such an executable needs to link when one of its objects has an exception landing pad, and nothing else.

The guest `libc` provides `__gxx_personality_v0`; a host-only link doesn't get it. On a DWARF-exception MinGW toolchain an object with a landing pad then fails to link. Test executables defined in the root `CMakeLists.txt` do not inherit that policy, so they do not need the helper; nor do test executables that link a guest library, or non-MinGW hosts.
