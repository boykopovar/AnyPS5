# libSceNKWebKit

## add Functional

- [WebCore::AdClickAttribution guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/AdClickAttribution.cpp) - console behaviour on invalid, unmapped, or read-only guest pointers (including nested `WTF::StringImpl` headers and character buffers) passed to `AdClickAttribution::parseConversionRequest`, `convertAndGetEarliestTimeToSend`, `hasHigherPriorityThan`, `url`, `urlForTesting`, `referrer`, `markAsExpired`, `hasExpired`, `markConversionAsSent`, `wasConversionSent`, and `toString` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers) without mutating guest output slots. Closes when: console behaviour on invalid guest pointers in these exports is measured.
