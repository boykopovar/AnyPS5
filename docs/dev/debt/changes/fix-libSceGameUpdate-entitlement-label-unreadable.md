# libSceGameUpdate

## change [sceGameUpdateGetAddcontLatestVersion

- [sceGameUpdateGetAddcontLatestVersion](../../../core/libs/prx/libSceGameUpdate/Export.cpp): the supplied module confirms a 20-byte entitlement-label input and a 48-byte service output. Null pointers are rejected before initialization is checked. There is no local output-size validation or blanket clearing of the output; answers `found = false` and returns 0 without contacting the patch service, matching `sceGameUpdateCheck`. An unreadable non-null entitlement-label parameter or unwritable non-null result pointer returns `0x80412803` after initialization instead of faulting the host; this host-safety fallback is unmeasured on a console.
