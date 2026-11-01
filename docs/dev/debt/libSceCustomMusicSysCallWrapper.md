# libSceCustomMusicSysCallWrapper technical debt

### Unknown function info

- [sceCustomMusicNetInetNtop](../../../core/libs/prx/libSceCustomMusicSysCallWrapper/Export.cpp) - signature and behavior are unknown; the parameterless int declaration is an import-resolution placeholder and throws.
- [sceCustomMusicNetShutdown](../../../core/libs/prx/libSceCustomMusicSysCallWrapper/Export.cpp) - signature and behavior are unknown; the parameterless int declaration is an import-resolution placeholder and throws.
- [sceCustomMusicNetSocketAbort](../../../core/libs/prx/libSceCustomMusicSysCallWrapper/Export.cpp) - signature and behavior are unknown; the parameterless int declaration is an import-resolution placeholder and throws.
- [sceCustomMusicPthreadAttrGet](../../../core/libs/prx/libSceCustomMusicSysCallWrapper/Export.cpp) - signature and behavior are unknown; the parameterless int declaration is an import-resolution placeholder and throws.
- [sceCustomMusicPthreadSelf](../../../core/libs/prx/libSceCustomMusicSysCallWrapper/Export.cpp) - signature and behavior are unknown; the parameterless int declaration is an import-resolution placeholder and throws.

- [ and sceCustomMusicSelect](../../../core/libs/prx/libSceCustomMusicSysCallWrapper/Export.cpp) (libSceCustomMusicSysCallWrapper) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCustomMusicSysCallWrapper.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
