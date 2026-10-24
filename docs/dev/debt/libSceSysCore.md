# libSceSysCore technical debt

### Unknown function info

- [sceApplicationGetAppInfoByAppId](../../../core/libs/prx/libSceSysCore/Export.cpp) takes a 32-bit application ID and a writable 152-byte record. The service fills the record through IPC; its complete field layout remains unresolved. The SystemService caller consumes a subrecord beginning at byte 48.
