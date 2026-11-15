# libSceIcu

## change [ICU calendar exports]

- [Calendar state](../../../core/libs/prx/libSceIcu/Calendar.cpp) has unmeasured console ABI and data-version equivalence. Open, close, clone, field queries, millisecond assignment and Gregorian cutover use pinned ICU 78.1 [ucal.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/ucal.cpp), including its 24 fields and invalid-zone GMT fallback. Null zone/locale refuses unknown guest defaults; initial time uses the host ICU clock. Unusable guest memory and foreign/stale handles throw. Native calendars free on close; distinct token identities remain until process teardown. ICU cleanup/reconfiguration require no live handles or concurrent use. Canonical/default-zone, keyword-enumeration and time-zone-display exports still throw after their existing early returns. Closes when: compared with console signatures, clocks, defaults and data, and remaining paths are implemented.
