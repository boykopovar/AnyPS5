# libSceNKWebKit

## add Functional

- [WebCore::ParsedContentType guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/ParsedContentType.cpp) - console behaviour on invalid, unmapped, or read-only guest pointers (including nested `WTF::StringImpl` headers/character buffers, parameter-name vectors, and parameter hash tables) passed to `ParsedContentType::create` and `ParsedContentType::serialize` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers/invalid mode) without mutating guest outputs or leaking allocations. Closes when: console behaviour on invalid guest pointers in these exports is measured.
