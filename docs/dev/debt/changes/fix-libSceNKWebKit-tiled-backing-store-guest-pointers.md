# libSceNKWebKit

## add Functional

- [TiledBackingStore guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/TiledBackingStore.cpp) (libSceNKWebKit) - `WebCore::TiledBackingStore::mapToContents` validates guest `self` and `rect` inputs with checked reads and calls `NotImplementedAt_nid_no_patch` on unmapped or unreadable guest memory. Console fault behavior is unmeasured. Closes when: console behavior for invalid guest pointers in `WebCore::TiledBackingStore::mapToContents` is verified on hardware.
