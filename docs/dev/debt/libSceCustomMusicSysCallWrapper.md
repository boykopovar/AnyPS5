# libSceCustomMusicSysCallWrapper technical debt

### Unknown function info

- [sceCustomMusicSelect](../../../core/libs/prx/libSceCustomMusicSysCallWrapper/Export.cpp) (libSceCustomMusicSysCallWrapper) - name and NID are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCustomMusicSysCallWrapper.c), but no credible prototype was found. The declaration has no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
