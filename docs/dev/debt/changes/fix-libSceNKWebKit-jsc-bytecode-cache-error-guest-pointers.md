# libSceNKWebKit

## add Functional

- [JscBytecodeCacheError guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/JscBytecodeCacheError.cpp) (libSceNKWebKit) - `JSC::BytecodeCacheError::isValid`, `operator=(const StandardError&)`, and `operator=(const WriteError&)` validate guest `BytecodeCacheError`, `StandardError`, `WriteError`, and releasable `WTF::StringImpl` pointers with checked guest reads/writes and call `NotImplementedAt_nid_no_patch` on unmapped or unreadable/unwritable guest memory. Console fault behavior is unmeasured. Closes when: console behavior for invalid guest pointers in `JSC::BytecodeCacheError` exports is verified on hardware.
