# libSceNKWebKit

## add Functional

- [SecurityOriginDataEquality guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/SecurityOriginDataEquality.cpp) (libSceNKWebKit) - `WebCore::operator==(const SecurityOriginData&, const SecurityOriginData&)` validates guest `SecurityOriginData`, nested `WTF::StringImpl` headers, and character buffers with checked guest reads and calls `NotImplementedAt_nid_no_patch` on unmapped or unreadable guest memory. Console fault behavior is unmeasured. Closes when: console behavior for invalid guest pointers in `WebCore::operator==(const SecurityOriginData&, const SecurityOriginData&)` is verified on hardware.
