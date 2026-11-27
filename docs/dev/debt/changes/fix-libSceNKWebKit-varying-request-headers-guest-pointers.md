# libSceNKWebKit

## add Functional

- [WebCore::VaryingRequestHeaders and SameSiteInfo guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/VaryingRequestHeaders.cpp) - console behaviour on invalid, unmapped, or read-only guest pointers (including nested `CookieJar` vtables, `HTTPHeaderMap` vectors, `WTF::Vector` buffers, and `WTF::StringImpl` storage) passed to `collectVaryingRequestHeaders`, `verifyVaryingRequestHeaders`, and `SameSiteInfo::create` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers) without mutating guest flags. Closes when: console behaviour on invalid guest pointers in these exports is measured.
