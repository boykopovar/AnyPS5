# libSceLoginService

## change [sceLoginServiceRequestDevices]

- [sceLoginServiceRequestDevices](../../../core/libs/prx/libSceLoginService/Export.cpp): the native wrapper confirms one read-only request pointer and transfers exactly 64 bytes to the login service. Null or unmapped input returns 0x813a0003 before the initialization check, preserving caller `errno`; an uninitialized service returns 0x813a0001. The request fields are interpreted by the service backend and remain unresolved.
