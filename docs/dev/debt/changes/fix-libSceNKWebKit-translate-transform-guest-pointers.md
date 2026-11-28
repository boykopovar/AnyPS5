# libSceNKWebKit

## add Functional

- [WebCore::TranslateTransformOperation guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/TranslateTransformOperation.cpp) (libSceNKWebKit) - unreadable guest `TranslateTransformOperation`, `TransformOperation`, `Length`, or `FloatSize` inputs and unwritable `TranslateTransformOperation`, `TransformationMatrix`, `TextStream`, or refcount targets are rejected via `NotImplementedAt_nid_no_patch` before mutating guest state rather than faulting. Console fault behavior is unmeasured. Closes when: console behavior on invalid guest pointers in these exports is measured.
