# libSceIcu

## add BiDi configuration and paragraph operations

- [BiDi configuration and paragraph operations](../../../core/libs/prx/libSceIcu/Bidi.cpp) implement `ubidi_openSized`, `ubidi_orderParagraphsLTR`, `ubidi_setInverse`, `ubidi_setReorderingMode`, `ubidi_setReorderingOptions`, `ubidi_setContext`, `ubidi_setLine`, `ubidi_getParagraph`, `ubidi_getParagraphByIndex`, `ubidi_getClassCallback` and `ubidi_getText` through bundled ICU 78.1. The console export availability, ABI and behavior are unverified. Null status pointers and BiDi objects throw where the console would fault. Closes when: compared with the console.
