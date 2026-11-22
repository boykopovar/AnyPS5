# libSceLibcInternal

## add Functional

- [_Getloctxt](../../../core/libs/prx/libSceLibcInternal/TimeGetFields.cpp) (libSceLibcInternal, NID `bsohl1ZrRXE`) - follows the reading of firmware 13.42's `libSceLibcInternal.sprx` in [#22452](https://github.com/boykopovar/AnyPS5/issues/22452#issuecomment-6971241155): two iterator references, a field count and a table whose first character marks each field, matched without regard to ASCII case. A table marked with NUL, such as the `\0false\0true` that `num_get` builds for `boolalpha`, takes its field count from the argument alone and starts each field at the next NUL, as that reading describes the field walk; no such table was run against the module. A count above the fields a table holds reads past the table on the console; here those fields are empty for a table with a visible mark, and a NUL-marked table must hold that many fields. Closes when: a NUL-marked table is run against the module.
