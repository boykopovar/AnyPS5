# libSceNKWebKit

## add Functional

- [WtfConcurrentPtrHashSet guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/WtfConcurrentPtrHashSet.cpp) (libSceNKWebKit) - `WTF::ConcurrentPtrHashSet` constructors, destructors, `clear`, `deleteOldTables`, and `addSlow` validate guest `ConcurrentPtrHashSet`, `ConcurrentPtrHashSetTable`, and table-list storage with checked guest reads/writes and call `NotImplementedAt_nid_no_patch` on unmapped or unreadable/unwritable guest memory. Console fault behavior is unmeasured. Closes when: console behavior for invalid guest pointers in `WTF::ConcurrentPtrHashSet` exports is verified on hardware.
