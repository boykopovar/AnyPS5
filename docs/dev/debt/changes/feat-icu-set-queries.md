# libSceIcu

## add Additional code point set queries

- [Additional code point set queries](../../../core/libs/prx/libSceIcu/Set.cpp) implement `uset_indexOf`, `uset_charAt`, `uset_getStringCount`, `uset_containsAll`, `uset_containsNone`, `uset_containsSome`, `uset_hasStrings`, `uset_isFrozen`, `uset_span`, `uset_spanBack` and `uset_spanUTF8` through the bundled ICU 78.1 `USet` implementation. The console export availability, ABI and behavior are unverified. Null and foreign set handles and null text with a non-zero length throw instead of reproducing ICU's undefined pointer access. Closes when: compared with the console.
