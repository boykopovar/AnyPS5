# libSceNKWebKit

## add Functional

- [WTF system and FileSystemImpl guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/WtfSystem.cpp) (libSceNKWebKit) - unreadable guest inputs to `WTF::constantTimeMemcmp`, `MappedFileData` constructors/destructors, and `FileSystemImpl` path exports (`openFile`, `makeAllDirectories`, `openAndLockFile`), as well as unwritable guest outputs to `WTF::cryptographicallyRandomValues`, `MappedFileData::mapFileHandle`/constructors, and `FileSystemImpl::closeFile`, are rejected via `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null `MappedFileData* this`) before mutating guest state or leaking mappings rather than faulting. Console fault behavior is unmeasured. Closes when: console behavior on invalid guest pointers in these exports is measured.
