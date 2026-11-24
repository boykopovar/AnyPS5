# libSceIcu

## add Set cloning, mutation and string operations

- [Set cloning, mutation and string operations](../../../core/libs/prx/libSceIcu/Set.cpp) implement `uset_clone`, `uset_cloneAsThawed`, `uset_freeze`, `uset_removeAll`, `uset_retainAll`, `uset_complementAll`, `uset_removeAllStrings`, `uset_addString`, `uset_removeString`, `uset_containsString` and `uset_addAllCodePoints` through the bundled ICU 78.1 `USet` implementation. The console export availability, ABI and behavior are unverified. Null and foreign set handles and null text with a non-zero length throw instead of reproducing ICU's undefined pointer access. Closes when: compared with the console.
