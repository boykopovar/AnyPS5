# libSceNKWebKit

## add Functional

- [WebCore::VaryingRequestHeaders and SameSiteInfo guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/VaryingRequestHeaders.cpp) - console behaviour on unreadable guest input ranges (including nested `CookieJar` vtables, `HTTPHeaderMap` vectors, `WTF::Vector` buffers, `URL` strings, and `WTF::StringImpl` storage) or an unwritable `LegacyResourceRequestBase` flags byte passed to `collectVaryingRequestHeaders`, `verifyVaryingRequestHeaders`, and `SameSiteInfo::create` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers), rolling back a failed flag write while preserving any flag write that already completed during an earlier header lookup. Closes when: console behaviour on invalid guest pointers in these exports is measured.
