# libSceSpSysCallWrapper technical debt

### Unknown function info

- [sceSpGetPid, sceSpSelect and sceSpPthreadMutexattrSetprioceiling](../../../core/libs/prx/libSceSpSysCallWrapper/Export.cpp) (libSceSpSysCallWrapper) - still throw. In firmware 13.60's `libSceSpSysCallWrapper.sprx`, `sceSpGetPid` and `sceSpSelect` are single jumps to libScePosix's `getpid` and `select`, and `sceSpPthreadMutexattrSetprioceiling` only jumps to libkernel's `scePthreadMutexattrSetprioceiling`, which AnyPS5 does not implement yet (read from disassembly, not measured on a console)
