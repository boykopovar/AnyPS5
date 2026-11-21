# libSceAbstractStorage

## add Functional

- [Serializable, MemfileContent and Item serialization calls](../../../core/libs/prx/libSceAbstractStorage/Export.cpp) (libSceAbstractStorage) - `Serializable::readExternal` and `Serializable::writeExternal` take a `shared_ptr<Content>` and answer `0x809E0005`; `Item::Rename`, `Item::GetThumbnail` and `Item::CreateThumbnail` also answer `0x809E0005`; `MemfileContent::readExternal` and `MemfileContent::writeExternal` answer `0x809E0002`. Those are the whole bodies in the 11.00 module (`mov eax, imm32; ret`, six bytes), so no argument is read and no state changes. Not measured on a console beyond the module.

# libSceAbstractTwitter

## add Functional

- [TwitterStorage and TwitterFolder calls](../../../core/libs/prx/libSceAbstractTwitter/Export.cpp) (libSceAbstractTwitter) - `TwitterFolder::RemoveFolder`, `RemoveContent`, `NextItem` and `NewFolder`, `TwitterContent::Rename`, and `TwitterStorage::GetContent`, `Deserialize`, `GetRootFolder`, `CreateRecursive`, `RemoveRecursive` and `Serialize` answer `0x809E0005` (`SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED`), and `TwitterStorage::IsExist` answers 0. Those are the whole bodies in the 11.00 module (`mov eax, imm32; ret` and `xor eax, eax; ret`), so no argument is read and no state changes; the rest of the class answers like its siblings. Not measured on a console beyond the module.
