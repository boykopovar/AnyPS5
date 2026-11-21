# libSceIcu

## change [ICU relative date-time formatter exports]

- [ICU relative date-time formatter exports](../../../core/libs/prx/libSceIcu/RelativeDateTimeFormatter.cpp) call the bundled ICU 78.1 `ureldatefmt_open`, `ureldatefmt_close`, `ureldatefmt_format` and `ureldatefmt_formatNumeric` APIs ([reldatefmt.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/reldatefmt.cpp#L1321-L1415)). The wrappers retain guest `UErrorCode` handling and copy formatted text through guest-memory helpers. The runtime embeds Unicode normalization data but not ICU locale data; without locale data registered by the application, opening a formatter returns `U_MISSING_RESOURCE_ERROR`. The test checks that reduced-data behavior and exercises formatting when locale data is available. The firmware reference is ICU 68.2; its locale data, formatting output and error behavior have not been compared with a PS5. Closes when: compared with the console.
