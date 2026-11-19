# libSceIcu

## change [ICU field position iterator exports]

- [ICU field position iterator exports](../../../core/libs/prx/libSceIcu/FieldPositionIterator.cpp) have unmeasured console ABI and ICU version equivalence. `ufieldpositer_open`, `ufieldpositer_close` and `ufieldpositer_next` use the pinned ICU 78.1 [ufieldpositer.cpp](https://github.com/unicode-org/icu/blob/049e0d6a4206/icu4c/source/i18n/ufieldpositer.cpp), [fpositer.cpp](https://github.com/unicode-org/icu/blob/049e0d6a4206/icu4c/source/i18n/fpositer.cpp) and [fphdlimp.cpp](https://github.com/unicode-org/icu/blob/049e0d6a4206/icu4c/source/i18n/fphdlimp.cpp). The `FieldPosition` support methods are ported from [format.cpp](https://github.com/unicode-org/icu/blob/049e0d6a4206/icu4c/source/i18n/format.cpp). Handles use host ICU objects; foreign handles and null status pointers throw. No guest formatter populates these iterators yet. Closes when: compared with the console.
