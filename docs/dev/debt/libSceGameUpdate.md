# libSceGameUpdate technical debt

### Functional

- [libSceGameUpdate](../../../core/libs/prx/libSceGameUpdate/Export.cpp): initialization tracks host state but does not connect the patch service. Request creation and entitlement version lookup throw instead of fabricating an offline error or a successful no-update result. The original maintains eight service-backed requests with IDs starting at `0x20000000`; the service's offline response is still unknown. Check, Abort and Delete implement argument/state checks but have no live requests to operate on.
- [sceGameUpdateCheck](../../../core/libs/prx/libSceGameUpdate/Export.cpp): option zero is supported for argument validation. The original additionally permits option one when a kernel platform predicate succeeds; that platform mode is not modeled.
- [sceGameUpdateGetAddcontLatestVersion](../../../core/libs/prx/libSceGameUpdate/Export.cpp): the supplied module confirms a 20-byte entitlement-label input and a 48-byte service output. Null pointers are rejected before initialization is checked. There is no local output-size validation or blanket clearing of the output; the host leaves it untouched when the missing service causes an exception.
