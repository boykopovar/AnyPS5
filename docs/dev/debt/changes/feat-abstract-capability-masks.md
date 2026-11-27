# libSceAbstractLocal

## change [Constant answers]

- [Constant answers](../../../core/libs/prx/libSceAbstractLocal/Export.cpp) (libSceAbstractLocal) - 10 methods return a constant whatever their arguments: 0 from `GetService`/`GetServiceType`, `0x37C5A` from `LocalContent::GetCapability`, `0x3FC000007E` from `LocalFolder::GetCapability`, `0xFC00000000001A` from `LocalStorage::GetCapability`, `0x809E0005` from `LocalContent::SeekTime` and `0x809E0002` from `LocalStorage::GetStat`/`GetStorageSize`; the two masks use the full 64-bit return register. Firmware 13.42 `libSceAbstractLocal.sprx` (SHA-256 `919db67c6e5f17c09743439335ee678f949d7358df816d48ac4a05cf77214b36`) uses `mov $<value>,%eax; ret` or `xor %eax,%eax; ret`; the masks are the same 11 bytes in 11.00 and 13.40. PS5 Pro 13.60 `dlopen` returned the first eight by name; the masks were not measured, and what the values stand for and the result types remain unverified.
