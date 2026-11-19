# libSceSystemService

## change [sceLncUtilBlockAppSuspend, sceLncUtilUnblockAppSuspend]

- [sceLncUtilBlockAppSuspend, sceLncUtilUnblockAppSuspend](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - AnyPS5 never suspends the title, so a block changes nothing. An unblock with no block left returns `SCE_KERNEL_ERROR_EBUSY` and changes nothing, before it reads the reason, as in [BunnyFl's measurements](https://github.com/boykopovar/AnyPS5/pull/9586#issuecomment-6362292920) of firmware 13.42's library. A null reason still throws `NotImplemented_nid_no_patch` in `sceLncUtilBlockAppSuspend`, where the library reads the reason only for a log line written below SDK 2.50 or with capability bit 62, a path that is not measured. In an unblock while a block is held the library faults on a null reason after lowering the count; here it throws `std::runtime_error` without lowering it
