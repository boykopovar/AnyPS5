# libSceShareFactoryUtil

## remove sceShareFactoryUtilBackToGameFromShareFactory

## add Functional

- [libSceShareFactoryUtil](../../../core/libs/prx/libSceShareFactoryUtil/Export.cpp) - ShareFactory VSH IPMI and event-flag IPC (`SceShareFactoryUtil%x`) is not emulated, so `sceShareFactoryUtilLaunchShareFactory`, `sceShareFactoryUtilBackToGameFromShareFactory`, `sceShareFactoryUtilGetLaunchStatus`, and `sceShareFactoryUtilGetLaunchStatusOfShareFactory` validate init state (`0x81630002`), null/guest pointers (`0x81630005`, `0x1100`-byte param, `0x114`-byte status) and return `0x81630003`. Field-level parameter/status layouts remain unverified.
