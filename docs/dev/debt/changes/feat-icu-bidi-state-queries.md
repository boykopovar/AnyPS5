# libSceIcu

## add BiDi state queries

- [BiDi state queries](../../../core/libs/prx/libSceIcu/Bidi.cpp) implement `ubidi_getDirection`, `ubidi_getLength`, `ubidi_getProcessedLength`, `ubidi_getResultLength`, `ubidi_getLevelAt`, `ubidi_getCustomizedClass`, `ubidi_getReorderingMode`, `ubidi_getReorderingOptions`, `ubidi_isInverse`, `ubidi_isOrderParagraphsLTR` and `ubidi_countParagraphs` through bundled ICU 78.1. The console export availability, ABI and behavior are unverified. Null BiDi objects throw where the console would fault. Closes when: compared with the console.
