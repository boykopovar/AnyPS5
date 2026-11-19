# libSceUlt

## change [Mutex and reader-writer status queries]

- [Mutex and reader-writer status queries](../../../core/libs/prx/libSceUlt/src/ReaderWriterLock.cpp) - not run on a console. From PS4's `libSceUlt.sprx` (SHA-256 `091005a21ac20d40436748315f3c7276b4f48277132a42b3774fa71b2b2b0d1f`, disassembly): `lockCount` counts holds only when attribute bit 0 (the only recursive bit) is set, else 0. An ulthread owner is `ownerType` 2 with the ulthread object's first 8 bytes, where create stores its name. [13.42 observations](https://github.com/boykopovar/AnyPS5/pull/9975#issuecomment-6417685562) (native module runs, not hardware) give `lockType` 1/2/0 for read/write/free, a pthread `owner` from the thread block's first 8 bytes with `ownerType` 1, and `lockCount` 0 for a default mutex held once. Both queries reject a waiting record whose `size` is not 64 with `ULT_ERROR_INVALID` before writing. Validation precedence and record-size checks of the other queries are unverified.
