# libSceSaveData.native

## add Functional

- [sceSaveDataSetSaveDataMemory2](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - recovery of a save-data memory update after power loss is unverified. A combined data and parameter update keeps both in its journal until the blob and parameter file have been written and flushed. Host directory entries are flushed on POSIX; on Windows their persistence relies on the filesystem.
