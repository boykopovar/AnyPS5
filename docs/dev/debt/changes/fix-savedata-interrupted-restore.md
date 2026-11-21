# libSceSaveData.native

## change a crash during a restore is not recovered at the next start

- [sceSaveDataRestoreBackupData](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - recovery on the console is unmeasured. A host restore that recorded its state can roll back at startup before commit or finish cleanup after commit. Artifacts left by older builds without a state file have no reliable commit status and are not replayed. Recovery after physical power loss is unverified.
