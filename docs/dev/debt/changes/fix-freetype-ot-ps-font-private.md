# libSceFreeTypeOt

## add Pending dependency exports

- [FT_Get_PS_Font_Private](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) (NID `+yNEa4OFH1M`) has an unverified console ABI and return codes. Windows forwards to the [libSceFreeType adapter](../../../core/libs/prx/libSceFreeType/src/PostScriptPrivate.cpp), which converts the native dictionary to the 224-byte LP64 record. For a null face, bundled FreeType returns Invalid_Face_Handle while 2.4.11 returns Invalid_Argument. The adapter rejects an inaccessible output as Invalid_Argument.
