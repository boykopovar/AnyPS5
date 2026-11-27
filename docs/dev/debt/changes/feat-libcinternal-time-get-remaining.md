# libSceLibcInternal

## remove [_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc]

## remove [_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc]

## remove [_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetintERS3_S5_iiRiRKSt5ctypeIcE]

## remove [_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc]

## add Functional

- [time_get remaining do_get_date, do_get_weekday, do_get, _Getfmt, and _Getint](../../../core/libs/prx/libSceLibcInternal/TimeGetFields.cpp) (libSceLibcInternal, NIDs `T85u2sPrKOo`, `xzYpD5d24aA`, `Vw03kdKZUN0`, `E7UermPZVcw`, `8raXTYQ11cg`, `SYCwZXKZQ08`, `hGlkh5YpcKw`, `0MzJAexrlr4`) are unverified on a console: the narrow overloads read static C-locale tables (`kCharDays`, `kCharAmPm`) rather than unconstructed storage past `+0x10`, while `g_wideTimeGetVtable` wires `do_get_date`, `do_get_weekday`, and `do_get` directly. Adapted from [xloctime](https://github.com/microsoft/STL/blob/770da5aa457af11f0a796600e1aa6f7f6fce14a6/stl/inc/xloctime#L289-L631).
