# libc

## add Functional

- [The narrow ctype functions outside -128 to 255](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc) - `isupper`, `islower`, `isalpha`, `isdigit`, `isalnum`, `isspace`, `isblank`, `iscntrl`, `isprint`, `isgraph`, `ispunct`, `isxdigit`, `isnumber`, `ishexnumber`, `toupper` and `tolower` throw `std::invalid_argument` for an argument below -128 or above 255, for example a wide character or a code point passed to `isdigit` or `tolower`. What the console returns there is not measured; C leaves it undefined. This library's `_Tolower`, `_Toupper`, `towlower` and `towupper` return such a value unchanged and the `isw*` functions return 0 for it. No test passes a value outside -128 to 255. Closes when: the console's results for those arguments are read.
