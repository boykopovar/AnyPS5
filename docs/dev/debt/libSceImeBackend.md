# libSceImeBackend technical debt

### Silent stubs

- [sceImeBackendForTestFunction](../../../core/libs/prx/libSceImeBackend/Export.cpp) (libSceImeBackend) - returns ErrorInternal (0x80bc00ff) unconditionally, matching sister export sceImeForTestFunction; test diagnostics backend remains unverified
- [sceImeBackendGetCandidateList, sceImeBackendGetCandidateListSize, sceImeBackendGetCandidateSelect](../../../core/libs/prx/libSceImeBackend/Export.cpp) (libSceImeBackend) - the host backend has no conversion engine, so it reports an empty candidate list and selection zero for every input. The module builds candidates from its dictionaries and IPMI client, which are not modelled.

### Functional

- [sceImeBackendGetStatus and the status checks](../../../core/libs/prx/libSceImeBackend/Export.cpp) (libSceImeBackend) - `status` stays 0 in this tree because every conversion call throws, so `GetStatus` answers 1 while the backend is open and the `status >= 2` branches in EnterCharacter, BackspaceCharacter, GetCaretIndex, ModeCaretNext, ModeCaretPrevious, GetCandidateList, GetCandidateListSize, GetCandidateSelect and DeleteCharacter cannot be reached by any test. The 1-to-4 mapping and the errors those branches return come from the 11.00 module, not from a console measurement.

- [`sceImeBackendParamInit`](../../../core/libs/prx/libSceImeBackend/Export.cpp) (libSceImeBackend) - a parameter block at an unmapped guest address is left unwritten and the call returns without an error; the console's result is not measured.

### Unknown function info

- [sceImeBackendParamInit](../../../core/libs/prx/libSceImeBackend/Export.cpp) (libSceImeBackend) - the layout of the 40-byte parameter block is unknown. It follows firmware 13.42's libSceImeBackend.sprx, read from its disassembly and run natively on x86-64 (not a console): it takes one pointer and returns nothing; a null pointer is ignored; otherwise it writes 0xFFFFFFFF as a 32-bit value at byte 0, clears bytes 4 to 39 and leaves later bytes unchanged
- [sceImeBackendOpen, sceImeBackendClose, sceImeBackendReset, sceImeBackendGetStatus, sceImeBackendGetString, sceImeBackendGetConvertString, sceImeBackendGetConfirmString, sceImeBackendGetCaretIndex, sceImeBackendModeCaretNext, sceImeBackendModeCaretPrevious, sceImeBackendGetConvertMode, sceImeBackendSetConvertMode, sceImeBackendBackspaceCharacter, sceImeBackendDeleteCharacter, sceImeBackendAllDeleteConvertString, sceImeBackendEnterCharacter](../../../core/libs/prx/libSceImeBackend/Export.cpp) (libSceImeBackend) - signatures and errors come from the 11.00 module disassembly, not a console measurement. The 0x28-byte block has mode at 0, max length at 4, type at 8, initial string at 0x10, field at 0x18 and 12 reserved bytes at 0x1c; Open validates them with the module's 0x80BC10xx errors and keeps one host text buffer. EnterCharacter only inserts code points up to 0x7f and throws above that; conversion is not modelled. The mode whitelist and status handling are approximations.

- [sceImeBackendConvertForward, sceImeBackendConvertBackward, sceImeBackendConvertCancel, sceImeBackendPartConfirm, sceImeBackendAllConfirm, sceImeBackendTemporalConfirm](../../../core/libs/prx/libSceImeBackend/Export.cpp) (libSceImeBackend) - the conversion engine, dictionary files and IPMI client used by the 11.00 module are not modelled; every call throws through `NotImplemented_nid_no_patch`. The module also imports `sceUserServiceGetLoginUserIdList` and reads dictionaries through `libkernel`, so the resulting strings and candidate ordering are unverified.
