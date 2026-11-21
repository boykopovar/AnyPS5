# libSceIcu

## add Functional

- [Character layout orientation](../../../core/libs/prx/libSceIcu/RightToLeft.cpp) (libSceIcu) - `uloc_getCharacterOrientation` follows ICU 78.1 layout resolution without external resource data: it resolves RTL languages and explicit scripts to `ULOC_LAYOUT_RTL`, and defaults all other valid locales (including root, empty, and undetermined) to `ULOC_LAYOUT_LTR`. Malformed locales or null/failure status inputs return `ULOC_LAYOUT_UNKNOWN` without modifying status. Console behavior is unverified. Closes when: compared with a console.

## remove pMLs993oHk0
