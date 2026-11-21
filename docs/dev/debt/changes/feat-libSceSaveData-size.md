# libSceSaveData.native

## add Unknown function info

- [sceSaveDataGetSaveDataCount, sceSaveDataGetAllSize](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - the first counts directories under the host save root, the second sums their files in KiB rounded up. Both ignore the first argument and write through the second, like [SharpEmu](https://github.com/sharpemu/sharpemu/blob/4d5697892e70f1d508ae6dbddf1e4b518c97fd84/src/SharpEmu.Libs/SaveData/SaveDataExports.cs#L509) and [RPS5E](https://github.com/keklick1337/rps5e/blob/d540c46f5d83efac3a0df9b2edc14cf12fe9f2be/crates/ps5hle/src/savedata.rs#L1473-L1497); that argument is unverified. A null or unwritable output returns `0x809F0000`, an uninitialized call `0x809F0001` and a host read failure `0x809F000B`. Those results, the KiB rounding and the sidecar exclusion are this implementation's choices; the console's count and size are unmeasured.
