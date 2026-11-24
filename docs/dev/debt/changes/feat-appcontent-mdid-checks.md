# libSceAppContent

## change sceAppContentUnknown_SWVxsi_ZBlw](

- [sceAppContentUnknown_SWVxsi_ZBlw](../../../core/libs/prx/libSceAppContent/Export.cpp) (libSceAppContent, NID SWVxsi-ZBlw) - master data identifier query (UnknownMdid in [dlc_emu](https://github.com/drakmor/dlc_emu/blob/fcf69b16d46a9850578bce19dc2c1b4dc867cbb5/src/dlc_modules/dlc_content.cpp#L2258-L2262)). Its checks follow a [reading of firmware 13.42's libSceAppContent.sprx](https://github.com/boykopovar/AnyPS5/issues/21875#issuecomment-6946781143), not measured on a console: `0x80D90001` before `sceAppContentInitialize`, then `0x80D90002` for a null 16-byte identifier or a null 1-byte result. Past them the module sends the identifier to the `SceAppContent` service, whose comparison isn't known, so the call throws. The usage count the module also checks before initialization is not traced; `main`'s initialized flag stands for both.
