# libSceLibcInternal

## add Functional

- [time_get fields, _Getint, _Getfmt, and get](../../../core/libs/prx/libSceLibcInternal/TimeGetFields.cpp) (libSceLibcInternal, NIDs `73GV+sRHbeY`, `dSfKN47p6ac`, `ZuCHPDq-dPw`, `2pJJ0dl-aPQ`, `cRSJysDpVl4`, `R1ITHuTUMEI`, `B8c4P1vCixQ`, `r8003V6UwZg`, `lhJWkEh-HXM`) are unverified on a console: `time_get<char>::do_get_monthname` reads the static C-locale month table instead of unconstructed storage past `+0x10`, and `time_get<wchar_t>` `get`/`_Getfmt` dispatch `%` specifiers through `vtable->get` which throws on C1/C2 facets while `do_get` (`0MzJAexrlr4`) remains stubbed. Adapted from [xloctime](https://github.com/microsoft/STL/blob/770da5aa457af11f0a796600e1aa6f7f6fce14a6/stl/inc/xloctime#L23-L633) and [`_Getloctxt`](https://github.com/microsoft/STL/blob/770da5aa457af11f0a796600e1aa6f7f6fce14a6/stl/inc/xlocale#L3272-L3315).
- [time_get and time_put constructors and destructors](../../../core/libs/prx/libSceLibcInternal/TimeGetFields.cpp) (libSceLibcInternal, NIDs `gOzIhGUAkME`, `XBmd6G-HoYI`, `dOKh96qQFd0`, `Zqc++JB04Qs`, `u5yK3bGG1+w`, `6NH0xVj6p7M`, `X3DrtC2AZCI`) are unverified on a console: `time_get<wchar_t>` writes `0x30` bytes (`_Cvt` past `+0x2c` unverified) with static C-locale UTF-16 tables, `mdy` (`2`), and throwing `do_get_date`/`do_get_weekday`/`do_get` vtable slots; `~time_get` D2 keeps static tables unfreed; `time_put<char>` fills `0xb8` bytes from `_Getptimes`; `time_put<wchar_t>` C1 writes only the `0x10`-byte header and does not call `_Init` (`ZolDcuDSD0Q`), which still throws because `_Tnames` past `+0x10` is unverified. Based on [xloctime](https://github.com/microsoft/STL/blob/770da5aa457af11f0a796600e1aa6f7f6fce14a6/stl/inc/xloctime#L130-L815).

## remove [_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm]

## remove [_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev]

## remove [_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom]

## remove [_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em]

## remove [_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev]

## remove [_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em]

## remove [_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2EPKcm]

## remove [_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm]

## remove [_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE_]

## remove [_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc]

## remove [_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE]
