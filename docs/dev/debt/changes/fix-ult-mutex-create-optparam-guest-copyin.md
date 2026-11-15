# libSceUlt

## add Functional

- [sceUltMutexCreate and _sceUltMutexCreate optParam](../../../core/libs/prx/libSceUlt/Export.cpp) (libSceUlt) - returning ULT_ERROR_NULL with caller errno preserved when optParam points to unreadable guest memory via GuestCopyIn_nid_no_patch is a host-safety fallback; hardware fault behavior on unmapped memory is unmeasured.
