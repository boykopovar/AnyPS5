# libfreetype

## add Functional

- [FT_Library_SetLcdGeometry and FT_Bitmap_Blend library handle](../../../core/libs/prx/libfreetype/src/LibraryHelpers.cpp) (libfreetype) - read `library->host` through `GuestCopyIn_nid_no_patch` while preserving `errno`, returning `FT_Err_Invalid_Library_Handle` in `FT_Library_SetLcdGeometry` and `FT_Err_Invalid_Argument` in `FT_Bitmap_Blend` on an unreadable `GuestLibrary` pointer; those fallback errors are host-safety guards and have not been measured on a console.
