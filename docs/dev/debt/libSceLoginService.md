# libSceLoginService technical debt

### Silent stubs

- [sceLoginServiceInitialize, sceLoginServiceTerminate](../../../core/libs/prx/libSceLoginService/Export.cpp) (libSceLoginService) - return success and keep no login-service state: AnyPS5 has no PSN login service. EA SPORTS FC 27 (PPSA34015) loads the module with `sceSysmoduleLoadModule(0xe3)` and calls `sceLoginServiceInitialize` with no arguments; a missing `libSceLoginService.prx` stops Windows start-up with `Failed to load module` / `GetLastError: 126`

### Unknown function info

- [sceLoginServiceRequestDevices](../../../core/libs/prx/libSceLoginService/Export.cpp): the native wrapper confirms one read-only request pointer and transfers exactly 64 bytes to the login service. Null input returns 0x813a0003 before the initialization check; an uninitialized service returns 0x813a0001. The request fields are interpreted by the service backend and remain unresolved. The protected export body is unchanged.
