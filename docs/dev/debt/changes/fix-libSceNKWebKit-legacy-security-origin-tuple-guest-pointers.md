# libSceNKWebKit

## add Functional

- [LegacySecurityOriginTuple guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/LegacySecurityOriginTuple.cpp) (libSceNKWebKit) - `WebCore::SecurityOriginData::fromDatabaseIdentifier` validates guest `result` output storage, `WTF::String`, `WTF::StringImpl`, and character buffers with checked guest reads/writes and calls `NotImplementedAt_nid_no_patch` on unmapped or unreadable/unwritable guest memory. Console fault behavior is unmeasured. Closes when: console behavior for invalid guest pointers in `WebCore::SecurityOriginData::fromDatabaseIdentifier` is verified on hardware.
