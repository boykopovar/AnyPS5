# libSceIcu

## add Functional

- [UnicodeSet code-point, string and property operations](../../../core/libs/prx/libSceIcu/Set.cpp) (libSceIcu) - `uset_complementAllCodePoints`, `uset_complementString`, `uset_containsAllCodePoints`, `uset_getRangeCount`, `uset_getString`, `uset_removeAllCodePoints`, `uset_resemblesPattern`, `uset_retainAllCodePoints`, `uset_retainString`, `uset_set` and `u_stringHasBinaryProperty` delegate to bundled ICU 78.1 through the existing checked set wrapper. Null strings with nonzero lengths and invalid set handles throw `std::invalid_argument`; `uset_getString` writes the length through a checked guest copy and returns bundled ICU's internal string pointer. Console behavior and binary compatibility with ICU 68.2 are unverified. Closes when: compared with the console.
