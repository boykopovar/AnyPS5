# libSceFreeType

## add Functional

- [FT_Get_Module, FT_Get_Renderer, FT_Remove_Module, and FT_Add_Default_Modules](../../../core/libs/prx/libSceFreeType/src/ModuleManagement.cpp) copy guest `library->host` and `moduleName` pointers through `GuestCopyIn_nid_no_patch`, preserving `errno` and failing through `NotImplemented_nid_no_patch` on unreadable guest addresses.
