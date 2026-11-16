# libSceRtc

## change capping `date_time` reads at 4096 bytes

- [sceRtcParseRFC3339, sceRtcParseDateTime](../../../core/libs/prx/libSceRtc/Export.cpp) (libSceRtc) - returning `0x80B50002` (`SCE_RTC_ERROR_INVALID_POINTER`) for unreadable non-null `date_time` or unreadable/unwritable non-null `utc` pointers is a host-safety fallback, unmeasured on a console. Allocation failure while copying the date text returns `0x80B50007` (`SCE_RTC_ERROR_BAD_PARSE`), also unmeasured on a console.
