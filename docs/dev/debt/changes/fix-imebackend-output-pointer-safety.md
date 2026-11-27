# libSceImeBackend

## add Functional

- [sceImeBackendOpen, sceImeBackendEnterCharacter, sceImeBackendGetString, sceImeBackendGetConvertString, sceImeBackendGetConfirmString, sceImeBackendGetCandidateList, sceImeBackendGetCandidateSelect, sceImeBackendGetConvertMode](../../../core/libs/prx/libSceImeBackend/Export.cpp) - safely copies `param` and bounded `initialString` via `GuestCopyIn_nid_no_patch`, guards `GuestWritable_nid_no_patch` probes with `GuestArena::HostWrite`, and writes results via `GuestCopyOut_nid_no_patch` before committing edits so unwritable or unmapped pointers return `kNullArgument` preserving caller `errno` and state; console behavior is unmeasured on hardware. Closes when: measured on hardware. Labels: console, tested.
