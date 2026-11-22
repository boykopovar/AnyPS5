# libc

## change [Exception runtime on macOS]

- [Exception runtime on macOS](../../../core/libs/prx/libc/src/exception/Exports.cpp) (libc) - on macOS an exception that a library throws with a plain `throw` and that nothing catches skips the guest's `std::set_terminate` handler and the guest runtime's report: it ends in libc++abi's terminate, because the host keeps libc++abi there, while on Linux and Windows libc's runtime takes these throws. The `SIGALRM` expiry of `alarm` and `setitimer` throws through the guest runtime on macOS; `NotImplemented_nid_no_patch` cannot, since host code catches its `std::runtime_error`, so an unimplemented call inside a `noexcept` export bypasses the guest handler

## change [app0.zar]

- [app0.zar](../../../core/libs/prx/libc/src/AppArchive.cpp) (libc) - host [ZArchive](https://github.com/Exzap/ZArchive) packaging. Archive-only paths throw in ordinary path/module resolution, `chdir` and other calls without archive support. Libc `opendir` throws for merged directories, including `app0/`, and `fdopen` throws for archived descriptors. Damaged blocks throw on reads instead of returning `EIO`. On Windows reads across writable and protected private pages return partial progress where loose-file reads return `EINVAL`. Reads check each destination page because HostWrite only opens watched pages. Each [mapping](../../../core/libs/prx/libc/src/AppArchiveBacking.cpp) copies its page-rounded window, clipped at EOF, under the attachment lock, serializing reads, seeks and maps on duplicates. Linux requires sealed `memfd` storage and `/proc/self/fd`; Windows and macOS require temporary disk space.

# libkernel

## change [app0.zar files and directories]

- [app0.zar files and directories](../../../core/libs/prx/libkernel/File/src/ArchivedFile.cpp) (libkernel) - ordering and seek cookies are host packaging behavior, unmeasured on console. Directory snapshots refresh on rewind and disk entries shadow archive names under the archive component comparison. Virtual directory descriptors reject data I/O, mapping, locks, filesystem queries and metadata mutation before touching their null-device backing. Archived files are read-only and duplicate descriptors share their cursor. `lstat`, access and other ordinary path operations on archive-only entries remain unsupported. File mappings inherit Windows private snapshots and whole-page EOF rejection, Linux EOF faults, and macOS loose-file write-upgrade behavior. Backing resource failures preserve guest resource errors without installing a mapping, and damaged entries return `EIO` at `mmap`. The tests use synthetic archives and loose files.
