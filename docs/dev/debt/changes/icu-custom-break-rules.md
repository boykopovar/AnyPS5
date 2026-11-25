# libSceIcu

## add Functional

- [Custom break rules](../../../core/libs/prx/libSceIcu/BreakRules.cpp) have no console ABI or title verification. Rule syntax and Unicode properties use the pinned ICU 78.1/Unicode 17 backend; parity with ICU 68.2/Unicode 13 remains unverified. Status-vector and clone contracts follow ICU 68.2, including allocated-clone warnings, rejection of requested-locale metadata and empty locale IDs for custom rules. Native iterator identity remains shared with the C break-iterator prerequisite. The two binary-rule exports still throw; no cross-version serialized-rule compatibility is claimed. Closes when: compared with the console and its Unicode data version.
