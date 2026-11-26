# libc

## add Functional

- [`_Copy_file` on a zero-byte write](../../../core/libs/prx/libc/src/VersionedFilesystemCalls.cpp) (libc) - returns guest `EIO` when a nonempty write makes no progress. The copy can leave a partially written destination, as with a write error. Console behavior for a zero-byte write is unverified.
