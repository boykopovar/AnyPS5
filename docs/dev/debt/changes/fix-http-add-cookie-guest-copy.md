# libSceHttp

## add Functional

- [sceHttpAddCookie](../../../core/libs/prx/libSceHttp/src/Cookies.cpp) (libSceHttp) copies the cookie text in before parsing it, up to its terminator or its `size`, whichever comes first, in page-bounded chunks with `GuestCopyIn_nid_no_patch`. An unreadable cookie returns `ERROR_INVALID_VALUE`, as a null one does. This is a host-safety fallback: the console's result for an inaccessible cookie is not measured. Closes when: compared with the console.
