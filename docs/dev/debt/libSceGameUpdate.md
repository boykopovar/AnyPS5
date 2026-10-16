# libSceGameUpdate technical debt

### Unknown function info

- [sceGameUpdateGetAddcontLatestVersion](../../../core/libs/prx/libSceGameUpdate/Export.cpp) (libSceGameUpdate) - a null `entitlement_label` returns `0x80412803` following prosper. Bytes after `found` are zeroed as in KytyPS5; prosper leaves them untouched
