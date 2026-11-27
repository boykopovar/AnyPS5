# libSceNKWebKit

## add Functional

- [JSC::Yarr::RegularExpression guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/YarrRegularExpression.cpp) - console behaviour on unreadable guest inputs or unwritable guest targets/ref-counts passed to `JSC::Yarr::RegularExpression` destructors (`D1`/`D2`) and copy assignment (`operator=`) is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers) without mutating ref-counts or target slots. Closes when: console behaviour on invalid guest pointers in these exports is measured.
