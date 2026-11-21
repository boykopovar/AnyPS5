# libSceImeBackend

## add Functional

- [sceImeBackendOpen, sceImeBackendEnterCharacter, sceImeBackendGetString, sceImeBackendGetConvertString, sceImeBackendGetConfirmString, sceImeBackendGetCandidateList, sceImeBackendGetCandidateSelect, sceImeBackendGetConvertMode](../../../core/libs/prx/libSceImeBackend/Export.cpp) - guest input parameters and output buffers are validated with `GuestCopyIn_nid_no_patch` and `GuestWritable_nid_no_patch` before writing results via `GuestCopyOut_nid_no_patch`. On unwritable or unmapped pointers, functions return `kNullArgument` while preserving caller `errno` and leaving internal state unmodified.
