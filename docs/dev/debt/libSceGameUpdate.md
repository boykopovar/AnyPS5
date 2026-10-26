# libSceGameUpdate technical debt

### Silent stubs

- [sceGameUpdateCreateRequest, sceGameUpdateCheck](../../../core/libs/prx/libSceGameUpdate/Export.cpp) (libSceGameUpdate) - requests are kept in the module's 8 slots with ids from `0x20000000` (a ninth returns `0x80412806`, an aborted request's Check `0x80412807`), as [prosper](https://github.com/mattias800/prosper) reads the PS5 3.20 `libSceGameUpdate.sprx`. Check never asks a patch service: it always answers no update (`found` and the byte after it cleared, the rest of the result untouched, return 0), which is what the module returns when its service reports `0x80412883`; KytyPS5 also answers no update. What the service returns on an offline console is not measured. The answer only decides whether a title offers a patch. PPSA22408 aborted on the throw in sceGameUpdateCreateRequest at its title screen. Closes when: measured on a PS5

### Functional

- [libSceGameUpdate](../../../core/libs/prx/libSceGameUpdate/Export.cpp): initialization tracks host state but does not connect the patch service. Entitlement version lookup throws.
- [sceGameUpdateCheck](../../../core/libs/prx/libSceGameUpdate/Export.cpp): option zero is supported for argument validation. The original additionally permits option one when a kernel platform predicate succeeds; that platform mode is not modeled.
- [sceGameUpdateGetAddcontLatestVersion](../../../core/libs/prx/libSceGameUpdate/Export.cpp): the supplied module confirms a 20-byte entitlement-label input and a 48-byte service output. Null pointers are rejected before initialization is checked. There is no local output-size validation or blanket clearing of the output; the host leaves it untouched when the missing service causes an exception.
