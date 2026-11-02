# libSceShareFactoryUtil technical debt

### Functional

- [sceShareFactoryUtilInitialize, sceShareFactoryUtilTerminate](../../../core/libs/prx/libSceShareFactoryUtil/Export.cpp) (libSceShareFactoryUtil) - track atomic initialization state.

### Unknown function info

- [sceShareFactoryUtilBackToGameFromShareFactory, sceShareFactoryUtilGetLaunchStatus, sceShareFactoryUtilGetLaunchStatusOfShareFactory and sceShareFactoryUtilLaunchShareFactory](../../../core/libs/prx/libSceShareFactoryUtil/Export.cpp) (libSceShareFactoryUtil) - check initialization state and throw `NotImplemented_nid_no_patch`. Names and NIDs are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceShareFactoryUtil.c); declarations have no parameters solely to resolve imports without making an ABI claim.
