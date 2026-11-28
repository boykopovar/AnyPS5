# libSceFreeType

## add Functional

- [FT_Set_Renderer](../../../core/libs/prx/libSceFreeType/src/RendererSelection.cpp) (libSceFreeType) - reads the guest `library->host` handle and each `FT_Parameter` entry through `GuestCopyIn_nid_no_patch` while preserving `errno`, returning `FT_Err_Invalid_Argument` when a non-null `library` or parameter entry cannot be read from guest memory; that fallback error is a host-safety guard and has not been measured on a console.
