# libSceVideoCoreInterface technical debt

### Unknown function info

- The following pending declarations have unverified signatures and behavior; they are parameterless import-resolution declarations and throw through `NotImplemented_nid_no_patch`: [`sceVideoCoreIfInitializeInterface`](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp), [`sceVideoCoreIfMainLoop`](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp), [`sceVideoCoreIfSetFunctionTable`](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp).
- [sceVideoCoreIfSetFunctionTableUnlimited](../../../core/libs/prx/libSceVideoCoreInterface/Export.cpp) (libSceVideoCoreInterface) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVideoCoreInterface.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
