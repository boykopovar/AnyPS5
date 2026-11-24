# libSceIcu

## add Additional code point set operations

- [Additional code point set operations](../../../core/libs/prx/libSceIcu/Set.cpp) implement `uset_clear`, `uset_compact`, `uset_complement`, `uset_complementRange`, `uset_containsRange`, `uset_equals`, `uset_isEmpty`, `uset_remove`, `uset_removeRange`, `uset_retain` and `uset_size` through the bundled ICU 78.1 `USet` implementation. The console export availability, ABI and behavior are unverified. Null and foreign set handles throw instead of reproducing ICU's undefined pointer access. Closes when: compared with the console.
