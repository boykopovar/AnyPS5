# libSceNKWebKit

## add Functional

- [WebCore::AsyncFileStream guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/AsyncFileStream.cpp) (libSceNKWebKit) - unreadable guest `AsyncFileStream`, `FileStreamClient`/vtable, `WTF::String`/`StringImpl`/character storage, `Optional<WallTime>`, or `FileStreamOperationFunction`/callable/vtable inputs and unwritable `AsyncFileStream`, `FileStreamOperationFunction`, or `read` destination buffers are rejected via `NotImplementedAt_nid_no_patch` before mutating guest state or enqueueing background work (with `read` copying through a host buffer via `GuestCopyOut_nid_no_patch`) rather than faulting. Console fault behavior is unmeasured. Closes when: console behavior on invalid guest pointers in these exports is measured.
