# libSceIcu

## add Functional

- [Field iterator guest memory](../../../core/libs/prx/libSceIcu/FieldPositionIterator.cpp): console behavior for inaccessible status and output pointers is unverified. `ufieldpositer_open` and `ufieldpositer_next` throw when a required guest read or write fails. The output handling is adapted from pinned ICU 78.1 [ufieldpositer.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/ufieldpositer.cpp); a rejected output leaves the native iterator advanced past that field. Closes when: compared with the console.
