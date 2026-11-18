# libc

## change [wctomb_s, wcrtomb_s, mbstowcs_s and mbsrtowcs_s]

- [wctomb_s, wcrtomb_s, mbstowcs_s and mbsrtowcs_s](../../../core/libs/prx/libc/src/BoundsChecking.cpp) (libc) - `mbsrtowcs_s` was not compared with a title's `libc.prx`, and no test passes multibyte or invalid UTF-8 text to `mbstowcs_s` or `mbsrtowcs_s`. The four follow the console's `libc.prx` and convert in the UTF-8 C locale of `wctomb`, `wcrtomb` and `mbrtowc` described under Wide-character conversions; [#12595](https://github.com/boykopovar/AnyPS5/pull/12595) found no difference from the titles' `wctomb_s`, `wcrtomb_s` and `mbstowcs_s`, characters above 0xFF and multibyte input included

## change [wcsrtombs_s and wcstombs_s]

- [wcsrtombs_s and wcstombs_s](../../../core/libs/prx/libc/src/Multibyte.cpp) (libc) - no test reaches the `ERANGE` return for a multibyte character that does not fit the remaining `len` or buffer; the only test where such a character does not fit is `wcstombs_s` with a `len` below `dstsz`, which stops before it without error. `wcsrtombs_s` reports `ERANGE` there; the console's `libSceLibcInternal.sprx` 12.00 does the same in both cases ([#8855](https://github.com/boykopovar/AnyPS5/pull/8855)). The `SIZE_MAX >> 3` bound on sizes and limits and the constraint handler's messages follow the `libc.prx` PPSA07923 and PPSA05916 ship ([#12595](https://github.com/boykopovar/AnyPS5/pull/12595))
