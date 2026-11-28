# libSceShareFactoryUtil

## remove sceShareFactoryUtilBackToGameFromShareFactory

## add Functional

- [libSceShareFactoryUtil](../../../core/libs/prx/libSceShareFactoryUtil/Export.cpp) - Verified from firmware 13.60 `libSceShareFactoryUtil.sprx` (`0x4d0`/`0x920`, `0x4e0`/`0xb10`, `0x4f0`/`0xd0`, `0x5c0`/`0x150`, `0x700`/`0x1d0`, `0x7c0`/`0x240`): `Initialize`/`Terminate` maintain atomic refcounting; `LaunchShareFactory(const void* param, uint32_t option)` (`0x1100` + 4-byte scalar `esi`), `BackToGameFromShareFactory(const void* param)` (`0x1100`), `GetLaunchStatus(void* status, const void* param)`, and `GetLaunchStatusOfShareFactory(void* status, const void* param)` (`0x114`, `0x1100`) enforce init (`0x81630002`), null/guest pointers (`0x81630005`), and return `0x81630003` when VSH ShareFactory IPC is unavailable. Field-level layouts remain unverified.
