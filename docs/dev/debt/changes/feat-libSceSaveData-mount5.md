# libSceSaveData.native

## add Unknown function info

- [sceSaveDataMount5](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - forwards to `sceSaveDataMount3` with the same `SaveDataMount3` and `SaveDataMountResult` layouts, as [SharpEmu](https://github.com/sharpemu/sharpemu/blob/4d5697892e70f1d508ae6dbddf1e4b518c97fd84/src/SharpEmu.Libs/SaveData/SaveDataExports.cs#L260-L261) and [RPS5E](https://github.com/keklick1337/rps5e/blob/d540c46f5d83efac3a0df9b2edc14cf12fe9f2be/crates/ps5hle/src/savedata.rs#L1636) do. Whether the console's Mount5 takes a different parameter block is unverified, and its result for such a call is unmeasured.
