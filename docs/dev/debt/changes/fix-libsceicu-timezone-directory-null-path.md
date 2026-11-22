# libSceIcu

## change [Time zone files directory]

- [Time zone files directory](../../../core/libs/prx/libSceIcu/Configuration.cpp) is unverified on a console. `u_setTimeZoneFilesDirectory` and `u_getTimeZoneFilesDirectory` follow ICU 78.1 [putil.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/putil.cpp): a locked copy that a null path empties without an error, as the bundled ICU does, and the empty string for a failure status. The default is empty; `ICU_TIMEZONE_FILES_DIR` and the build-time default are not read, and nothing in this module reads time zone files. A null `UErrorCode` throws where ICU dereferences it. The NIDs come from the 3.20 inventory and their presence on current firmware is unproven (see ICU export availability). Closes when: compared with the console.
