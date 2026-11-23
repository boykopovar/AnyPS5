# libSceIcu

## change [ICU number format exports]

- Console ABI/data-version and title behavior of [classic number formatting](../../../core/libs/prx/libSceIcu/NumberFormat.cpp) are unverified; its nine APIs follow pinned ICU78.1. NULL guest-default locales and foreign/stale handles throw. The older symbol range0-27 remains; native78 symbol28 is rejected. Bounded output staging calls native code repeatedly; only deterministic nonoverlapping writes are supported. Allocator/floating-environment side effects and native allocation-failure equivalence are unverified. Unknown native errors with possible partial output and overlapping output/status/field records throw. Other alias ordering and concurrent global cleanup/default changes are unverified. Native owners free after active calls finish; token identities persist to process exit.
