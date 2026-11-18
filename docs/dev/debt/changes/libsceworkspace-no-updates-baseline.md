# libSceWorkspace

## remove [module_start]

## remove [sceWorkspaceFinishWrite]

## remove [sceWorkspaceApplyUpdateForFiles]

## remove [sceWorkspaceApplyUpdateForAllFiles]

## remove [sceWorkspaceIsBlockedByDataTransfer]

## remove [sceWorkspaceMirrorBarrier]

## remove [sceWorkspaceGetNumberOfPendingUpdates]

## remove [module_stop]

## remove [sceWorkspaceApplyUpdate]

## remove [sceWorkspaceApplyUpdateForDirectory]

## remove [sceWorkspaceIsUpdatePending]

## add Unknown function info

- [11 exports now return 0 as the retail no-op baseline](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - `module_start_nid_postfix`, `module_stop_nid_postfix`, `sceWorkspaceFinishWrite`, `sceWorkspaceApplyUpdate`, `sceWorkspaceApplyUpdateForAllFiles`, `sceWorkspaceApplyUpdateForDirectory`, `sceWorkspaceApplyUpdateForFiles`, `sceWorkspaceGetNumberOfPendingUpdates`, `sceWorkspaceIsBlockedByDataTransfer`, `sceWorkspaceIsUpdatePending` and `sceWorkspaceMirrorBarrier` return 0. No host workspace is attached, so no update is pending and no data transfer is blocking; the apply, finish and barrier entries have nothing to do; and the module start and stop entries succeed as the standard sce module hooks. The other seven exports (`sceWorkspaceCreateFile`, `sceWorkspaceDeletePath`, `sceWorkspaceGetName`, `sceWorkspaceGetPendingUpdateEntries`, `sceWorkspaceMount`, `sceWorkspaceUnmount` and `sceWorkspaceWriteFile`) still throw because they operate on files, paths, mount points or output buffers whose shape must be settled before they can be implemented.
