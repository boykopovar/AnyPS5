# libSceIcu

## change [ICU relative date-time formatter exports]

- [ICU relative date-time formatter exports](../../../core/libs/prx/libSceIcu/RelativeDateTimeFormatter.cpp) call the bundled ICU 78.1 `ureldatefmt_open`, `ureldatefmt_close`, `ureldatefmt_format` and `ureldatefmt_formatNumeric` APIs ([reldatefmt.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/reldatefmt.cpp#L1321-L1415)). The wrappers retain the guest `UErrorCode` handling and copy formatted text through guest-memory helpers. The firmware reference is ICU 68.2; its locale data, formatting output and error behavior have not been compared with a PS5. Closes when: compared with the console.
