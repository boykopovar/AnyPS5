# libSceAbstractTwitter

## change [Constant answers]

- [Constant answers](../../../core/libs/prx/libSceAbstractTwitter/Export.cpp) (libSceAbstractTwitter) - error name, meanings of `3` and `0x6C0A`, result types, and parameters (kept no-parameter) are unverified. Sixteen `TwitterContent`, `TwitterFolder`, `TwitterService` and `TwitterStorage` methods return constants (`0x809E0005` for `GetStat`, `Read`, `SeekTime`, `Flush`, `Remove`, `Open`, `Close`, `GetStorageSize`; `3` for `GetService`, `GetServiceType`; `0x6C0A` for `TwitterContent::GetCapability`). Firmware 13.42 `libSceAbstractTwitter.sprx` (`e851cc7a05f7d7451f89bc0d73b139257370d95010ba90275f00c9fcd705d228`) uses `mov $<value>,%eax; ret`; PS5 Pro firmware 13.60 `dlopen` confirmed all except `TwitterFolder::RemoveFolder`, `RemoveContent` and `TwitterStorage::RemoveRecursive` (unresolved by name).
