# libSceJsc

## add Functional

- [WTF::Unicode UTF-8 conversion](../../../core/libs/prx/libSceJsc/src/WtfUnicodeConversion.cpp) (libSceJsc) - not compared with a console module; the console WTF and ICU versions are unverified. `convertLatin1ToUTF8`, `convertUTF16ToUTF8`, `convertUTF8ToUTF16` and `convertUTF8ToUTF16ReplacingInvalidSequences` follow pinned [UTF8Conversion.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WTF/wtf/unicode/UTF8Conversion.cpp#L37-L128) with the `U8_`/`U16_` macros of ICU 78.1, whose ill-formed sequence handling has not changed since ICU 60. Throws instead of writing past the target end, where the console's unchecked ASCII (UTF-8) and BMP (UTF-16) appends overrun it, and for null cursors and ranges above `INT_MAX`, which upstream does not handle.

## remove | `_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b` | `HblNi06Y6UI` |

## remove | `_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb` | `UjTDE3AQCRg` |

## remove | `_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_` | `AJ7+0UjKEww` |

## remove | `_ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb` | `PqZUVw-1dv0` |
