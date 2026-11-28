# libSceNKWebKit

## add Functional

- [WebCore::PerspectiveTransformOperation guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/PerspectiveTransformOperation.cpp) - console behaviour on invalid, unmapped, or read-only guest pointers passed to `PerspectiveTransformOperation::create`, constructors (`C1`/`C2`), destructors (`D0`/`D1`/`D2`), `clone`, `isIdentity`, `isAffectedByTransformOrigin`, `perspective`, `apply`, and `operator==` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers) without mutating guest output slots or leaking allocations. Closes when: console behaviour on invalid guest pointers in these exports is measured.
