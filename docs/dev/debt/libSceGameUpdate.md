# libSceGameUpdate technical debt

### Silent stubs

- [sceGameUpdateCreateRequest, sceGameUpdateCheck](../../../core/libs/prx/libSceGameUpdate/Export.cpp) (libSceGameUpdate) - requests are kept in the module's 8 slots with ids from `0x20000000` (a ninth returns `0x80412806`, an aborted request's Check `0x80412807`), as [prosper](https://github.com/mattias800/prosper) reads the PS5 3.20 `libSceGameUpdate.sprx`. Check never asks a patch service: it always answers no update (`found` and the byte after it cleared, the rest of the result untouched, return 0), which is what the module returns when its service reports `0x80412883`; KytyPS5 also answers no update. What the service returns on an offline console is not measured. The answer only decides whether a title offers a patch. PPSA22408 aborted on the throw in sceGameUpdateCreateRequest at its title screen. Closes when: measured on a PS5

### Unknown function info

- [sceGameUpdateCheckTitle](../../../core/libs/prx/libSceGameUpdate/Export.cpp) has an unknown signature and behaviour. The parameterless `int` declaration is an import-resolution placeholder, not a confirmed ABI signature, and every call throws through `NotImplemented_nid_no_patch`. Only its name and NID `sGNKn-jXzHQ` are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscegameupdate-16da6af946.jsonl); not measured on a console.

### Functional

- [libSceGameUpdate](../../../core/libs/prx/libSceGameUpdate/Export.cpp): initialization tracks host state but does not connect the patch service. Entitlement version lookup throws.
- [sceGameUpdateCheck](../../../core/libs/prx/libSceGameUpdate/Export.cpp): option zero is supported for argument validation. The original additionally permits option one when a kernel platform predicate succeeds; that platform mode is not modeled. Three titles disassembled for this entry (PPSA01494 01.100.002 at 0x63f1982, PPSA30839 01.003.000 at 0x6bf0c92, PPSA31162 02.000.001 at 0x6a204e1) pass option zero: right after `sceGameUpdateCreateRequest` they fill both the parameter and the result with a 64-bit `size` of 0x30 and zeros after it. On a non-negative return they read only `found` (PPSA01494 tests it for nonzero, the other two for 1), never `addcont_found` or `content_version`; PPSA01494 and PPSA30839 then call `sceGameUpdateDeleteRequest` whatever the result.
- [sceGameUpdateGetAddcontLatestVersion](../../../core/libs/prx/libSceGameUpdate/Export.cpp): the supplied module confirms a 20-byte entitlement-label input and a 48-byte service output. Null pointers are rejected before initialization is checked. There is no local output-size validation or blanket clearing of the output; the host leaves it untouched when the missing service causes an exception.
