# libSceLibcInternal

## remove _ZSt10_GetloctxtIcSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT_

## remove _ZSt10_GetloctxtIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT_

## add Functional

- [_Getloctxt over wide input](../../../core/libs/prx/libSceLibcInternal/TimeGetFields.cpp) (libSceLibcInternal, NIDs `i4J5FvRPG-w`, `FX+eS2YsEtY`) - the `wchar_t` table and `char` table forms over an `istreambuf_iterator<wchar_t>` run the field walk of the `char` form (`bsohl1ZrRXE`) on 16-bit input and are not run against the module. The `char` table form compares each table byte with the 16-bit input element by integral value, the byte sign-extended as a signed `char` and both folded only in ASCII, as the template's `_Ptr[_Off] != *_First` promotes them; a table byte above 0x7f therefore matches no input element. Confidence in that mixed comparison is medium. Closes when: both forms are run against the module with a non-ASCII table byte.
