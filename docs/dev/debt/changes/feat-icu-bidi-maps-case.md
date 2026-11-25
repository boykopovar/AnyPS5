# libSceIcu

## add BiDi maps and UTF-8 case mapping

- [BiDi maps and UTF-8 case mapping](../../../core/libs/prx/libSceIcu/Bidi.cpp) implement `ubidi_getLevels`, `ubidi_getLogicalIndex`, `ubidi_getLogicalMap`, `ubidi_getVisualIndex`, `ubidi_getVisualMap`, `ubidi_invertMap`, `ubidi_reorderLogical`, `ubidi_writeReordered`, `ucasemap_open`, `ucasemap_utf8FoldCase` and `ucasemap_utf8ToUpper` through bundled ICU 78.1. The console export availability, ABI and behavior are unverified. Invalid guest output pointers throw where the console would fault. Closes when: compared with the console.
