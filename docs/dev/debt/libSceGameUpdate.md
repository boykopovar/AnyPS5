# libSceGameUpdate technical debt

### Silent stubs

- [sceGameUpdateCreateRequest, sceGameUpdateCheck, sceGameUpdateCheckTitle, sceGameUpdateGetAddcontLatestVersion](../../../core/libs/prx/libSceGameUpdate/Export.cpp) (libSceGameUpdate) - no patch service is asked. Check answers no update (`found` and the byte after it cleared, the rest untouched, return 0), what the module returns when its service reports `0x80412883`. sceGameUpdateGetAddcontLatestVersion clears `found` and returns 0 after its argument and initialization checks; its service path in the module is not read. Requests use the module's 8 slots with ids from `0x20000000` (a ninth returns `0x80412806`, an aborted request's Check `0x80412807`), as [prosper](https://github.com/mattias800/prosper) reads the PS5 3.20 `libSceGameUpdate.sprx`. Offline console answers are not measured. PPSA22408 aborted on the throw in sceGameUpdateCreateRequest, PPSA24263 on the one in sceGameUpdateGetAddcontLatestVersion. Closes when: measured on a PS5

### Unknown function info

- [sceGameUpdateCheckTitle](../../../core/libs/prx/libSceGameUpdate/Export.cpp) validates initialization and returns 0 without contacting the patch service; its signature and full behaviour remain unconfirmed on a console.

### Functional

- [libSceGameUpdate](../../../core/libs/prx/libSceGameUpdate/Export.cpp): initialization tracks host state but does not connect the patch service.
- [sceGameUpdateCheck](../../../core/libs/prx/libSceGameUpdate/Export.cpp): option zero is supported for argument validation. The original additionally permits option one when a kernel platform predicate succeeds; that platform mode is not modeled. Three titles disassembled for this entry (PPSA01494 01.100.002 at 0x63f1982, PPSA30839 01.003.000 at 0x6bf0c92, PPSA31162 02.000.001 at 0x6a204e1) pass option zero: right after `sceGameUpdateCreateRequest` they fill both the parameter and the result with a 64-bit `size` of 0x30 and zeros after it. On a non-negative return they read only `found` (PPSA01494 tests it for nonzero, the other two for 1), never `addcont_found` or `content_version`; PPSA01494 and PPSA30839 then call `sceGameUpdateDeleteRequest` whatever the result.
- [sceGameUpdateGetAddcontLatestVersion](../../../core/libs/prx/libSceGameUpdate/Export.cpp): the supplied module confirms a 20-byte entitlement-label input and a 48-byte service output. Null pointers are rejected before initialization is checked. There is no local output-size validation or blanket clearing of the output; answers `found = false` and returns 0 without contacting the patch service, matching `sceGameUpdateCheck`.
