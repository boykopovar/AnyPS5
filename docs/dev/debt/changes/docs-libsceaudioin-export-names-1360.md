# libSceAudioIn

## change 6 additional pending imports

- 6 additional pending imports in [InventoryExports.cpp](../../../core/libs/prx/libSceAudioIn/InventoryExports.cpp) are listed in the local PS5 3.20 export inventory `libSceAudioIn.c`. Their signatures and behaviors are unverified; each uses the no-parameter unknown-signature convention and throws through `NotImplemented_nid_no_patch`, for import resolution only. Measured on a PS5 Pro payload process (13.60): after `dlopen("libSceAudioIn.sprx")` every one of the six names resolves to null through `dlsym`, while all fourteen unknown-signature names of the section above resolve - these six are not exported by name anywhere in the 13.60 module, consistent with linking by NID only.

## change [sceAudioInChangeAppModuleState](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn)

- [sceAudioInChangeAppModuleState](../../../core/libs/prx/libSceAudioIn/Export.cpp) (libSceAudioIn) - unknown signature; the export throws through `NotImplemented_nid_no_patch`, and this parameterless declaration is for import resolution only. The closest signed sibling, `sceMbusChangeAppModuleState` (`std::uint32_t deviceClass`, `std::int32_t moduleId`, `std::uint32_t state`), takes three arguments, so this shape is probably not the real one. What the console notifies and whether a title calls it is unmeasured; measured on a PS5 Pro payload process (13.60) only that the name resolves through `dlsym` in the loaded module - like all thirteen sibling unknown-signature names probed in the same session - and it was not called, its signature being unknown.
