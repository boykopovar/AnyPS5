# libSceGameUpdate technical debt

### Unknown function info

- [sceGameUpdateGetAddcontLatestVersion](../../../core/libs/prx/libSceGameUpdate/Export.cpp) (libSceGameUpdate) - a null `entitlement_label` returns `0x80412803` as in prosper; KytyPS5 accepts it instead, and neither is confirmed on a console. Bytes after `found` are zeroed as in KytyPS5; prosper leaves them untouched
