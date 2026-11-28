# libSceSaveData.native

## change an unterminated directory name returns

- [sceSaveDataBackup](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - PS5 request validation, completion errors and timing are unmeasured. The copy runs before the call returns and uses the running title's host save directory. If the request names another title, the call returns `SAVE_DATA_ERROR_NOT_FOUND`. The initialization check, running-title default, mounted-save rejection and copy-failure completion follow [shadPS4's request](https://github.com/shadps4-emu/shadPS4/blob/ce74386283f559f2279a7c31416bcdc9359be0b3/src/core/libraries/save_data/savedata.cpp#L567-L593) and [worker](https://github.com/shadps4-emu/shadPS4/blob/ce74386283f559f2279a7c31416bcdc9359be0b3/src/core/libraries/save_data/save_backup.cpp#L167-L173). An invalid guest pointer or directory name returns `SAVE_DATA_ERROR_PARAMETER`. Progress is `0.0f` for a failed copy and `1.0f` for a successful copy.

## change still copies nothing

- [sceSaveDataCheckBackupData, sceSaveDataRestoreBackupData, sceSaveDataUmountWithBackup](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - the block layouts, error checks and behaviour follow shadPS4's PS4 binding ([check](https://github.com/shadps4-emu/shadPS4/blob/ce74386283f559f2279a7c31416bcdc9359be0b3/src/core/libraries/save_data/savedata.cpp#L615-L664), [restore](https://github.com/shadps4-emu/shadPS4/blob/ce74386283f559f2279a7c31416bcdc9359be0b3/src/core/libraries/save_data/savedata.cpp#L1360-L1398), [unmount with backup](https://github.com/shadps4-emu/shadPS4/blob/ce74386283f559f2279a7c31416bcdc9359be0b3/src/core/libraries/save_data/savedata.cpp#L1758-L1761)). This behavior is unmeasured on PS5.
