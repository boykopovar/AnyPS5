# libSceNetCtlAp technical debt

### Functional

- [libSceNetCtlAp](../../../core/libs/prx/libSceNetCtlAp/Export.cpp) (libSceNetCtlAp) - the access point never starts: a title starts it through [libSceNetCtlApDialog](libSceNetCtlApDialog.md), which always reports the user as having cancelled. `sceNetCtlApGetState` reports Stopped (0), `sceNetCtlApGetInfo`, `sceNetCtlApGetConnectInfo` and `sceNetCtlApStop` return `NOT_STARTED` (`0x8041298F`) without writing, and no callback ever fires. Signatures, states, structure sizes (152 and 184 bytes), the 8-callback limit and error codes come from [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/Net/NetCtlAp.cs)

### Unknown function info

- [sceNetCtlApInit, sceNetCtlApTerm](../../../core/libs/prx/libSceNetCtlAp/Export.cpp) (libSceNetCtlAp) - every other call returns `NOT_INITIALIZED` (`0x80412994`) before `sceNetCtlApInit` and after `sceNetCtlApTerm`, which also drops the registered callbacks. A second `sceNetCtlApInit` throws, as no error for it is documented
- [sceNetCtlApGetInfo, sceNetCtlApGetConnectInfo, sceNetCtlApStop](../../../core/libs/prx/libSceNetCtlAp/Export.cpp) - stopped-state results come from a service on the console and remain unverified. Wrapper validation is recovered: initialization precedes arguments, the optional connection string is at most eight bytes, then output pointers and exact 152/184-byte sizes are checked. The service interpretation of the option string remains unresolved.
- [sceNetCtlApGetResult](../../../core/libs/prx/libSceNetCtlAp/Export.cpp) - with no event delivered, the host writes zero; the service result before the first event remains unknown. Wrapper validation checks initialization, output pointer and event type in that order. Event types 1 through 4 and lowest-free callback IDs 0 through 7 are confirmed.
