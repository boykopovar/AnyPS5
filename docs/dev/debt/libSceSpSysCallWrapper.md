# libSceSpSysCallWrapper technical debt

### Unknown function info

- [sceSpPthreadMutexattrSetprioceiling](../../../core/libs/prx/libSceSpSysCallWrapper/Export.cpp) (libSceSpSysCallWrapper) - still throws. In firmware 13.60's `libSceSpSysCallWrapper.sprx` it only jumps to libkernel's `scePthreadMutexattrSetprioceiling`, which AnyPS5 does not implement yet (read from disassembly, not measured on a console)
