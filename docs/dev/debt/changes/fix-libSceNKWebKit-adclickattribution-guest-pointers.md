# libSceNKWebKit

## add Functional

- [WebCore::AdClickAttribution guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/AdClickAttribution.cpp) - console behaviour on unreadable guest input ranges (including nested `WTF::StringImpl` headers and character buffers) and unwritable guest output ranges passed to `AdClickAttribution::parseConversionRequest`, `convertAndGetEarliestTimeToSend`, `hasHigherPriorityThan`, `url`, `urlForTesting`, `referrer`, `markAsExpired`, `hasExpired`, `markConversionAsSent`, `wasConversionSent`, and `toString` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers) without leaving partial writes in guest output slots. Closes when: console behaviour on invalid guest pointers in these exports is measured.
