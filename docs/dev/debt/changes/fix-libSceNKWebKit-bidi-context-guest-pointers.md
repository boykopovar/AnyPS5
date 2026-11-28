# libSceNKWebKit

## add Functional

- [WebCore::BidiContext guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/BidiContext.cpp) (libSceNKWebKit) - unreadable or cyclic guest `BidiContext` parent chains, unwritable `parent` objects whose reference counts would be updated, and unwritable `BidiContextRef` return storage are rejected via `NotImplementedAt_nid_no_patch` before mutating return storage (with RAII refcount/allocation rollback if publishing fails) rather than faulting or looping. Console fault behavior is unmeasured. Closes when: console behavior on invalid guest pointers in these exports is measured.
