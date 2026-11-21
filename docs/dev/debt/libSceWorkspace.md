# libSceWorkspace technical debt

### Unknown function info

The 18 exports in [Export.cpp](../../../core/libs/prx/libSceWorkspace/Export.cpp) come from the `libSceWorkspace` list in [claimore22/ps5rs](https://github.com/claimore22/ps5rs/blob/5f252a658ff470bcb2b6cd15205fd8ca64ae4477/data/stubs/by_library/libSceWorkspace_nosubmission.txt) (commit 5f252a658ff470bcb2b6cd15205fd8ca64ae4477), a catalogue of PS5 symbols per library whose provenance the project does not document, and no PS5 export inventory of this module is available to confirm the list is complete or exact. Every name was kept only because its hash equals the listed NID. Prototypes and behaviour are unverified: every function takes no parameters in the import-resolution convention and throws through `NotImplemented_nid_no_patch`.

- [module_start](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID BaOKcng8g88)
- [sceWorkspaceWriteFile](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID DYgq0+kM8UI)
- [sceWorkspaceFinishWrite](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID Ej722ud9Zo4)
- [sceWorkspaceApplyUpdateForFiles](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID FLTTgSdkoOc)
- [sceWorkspaceApplyUpdateForAllFiles](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID GKMjPmDls-w)
- [sceWorkspaceIsBlockedByDataTransfer](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID GgrAun5x5Eg)
- [sceWorkspaceMirrorBarrier](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID HcbIPAn1o3g)
- [sceWorkspaceGetNumberOfPendingUpdates](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID JvThkOI0jjE)
- [module_stop](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID KpDMrPHvt3Q)
- [sceWorkspaceUnmount](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID SOh2WS6VvIE)
- [sceWorkspaceApplyUpdate](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID WPBiUIzH4z4)
- [sceWorkspaceDeletePath](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID bSJy+Zr5h+M)
- [sceWorkspaceMount](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID bsUfUrKJ4Qo)
- [sceWorkspaceGetName](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID d9XUi2-zgWw)
- [sceWorkspaceCreateFile](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID euJjtMph7f4)
- [sceWorkspaceApplyUpdateForDirectory](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID hqY8PDT4oqU)
- [sceWorkspaceIsUpdatePending](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID t0z87PlggWI)
- [sceWorkspaceGetPendingUpdateEntries](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID wvc0E5cTehw)
