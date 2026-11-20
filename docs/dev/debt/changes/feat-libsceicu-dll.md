# libSceIcu

## add Functional

- [Dynamic loading exports](../../../core/libs/prx/libSceIcu/Dll.cpp) (libSceIcu) - `uprv_dl_open`, `uprv_dlsym_func` and `uprv_dl_close` are ported from ICU 68.2's `putil.cpp` over `LoadLibraryA`/`GetProcAddress`/`FreeLibrary` on Windows and `dlopen`/`dlsym`/`dlclose` elsewhere, with the guest library name read through `CopyGuestString` and the status word through the guest copy helpers: a missing library reports `U_MISSING_RESOURCE_ERROR` and a null name an illegal argument. `uprv_isASCIILetter` is the ASCII classification from the same file. The plain `NotImplemented` paths still answer `NotImplemented_nid_no_patch`.
