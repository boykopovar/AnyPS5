# libSceNKWebKit

## add Functional

- [WebCore::ParsedContentType guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/ParsedContentType.cpp) - console behaviour on unreadable guest input ranges (including nested `WTF::StringImpl` headers/character buffers, parameter-name vectors, and parameter hash tables) or unwritable guest output/ref-counted ranges passed to `ParsedContentType::create` and `ParsedContentType::serialize` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers/invalid mode). Closes when: console behaviour on invalid guest pointers in these exports is measured.
