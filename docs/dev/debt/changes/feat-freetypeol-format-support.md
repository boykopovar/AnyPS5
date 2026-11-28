# libSceFreeTypeOl

## remove Composite format/module configuration

## add Silent stubs requiring Sony-specific or shared-state behavior

- [ModuleWrappers.cpp](../../../core/libs/prx/libSceFreeTypeOl/ModuleWrappers.cpp) (libSceFreeTypeOl) - the eight `sceFreeTypeOlFtaSupportFormat*` helpers register the matching bundled FreeType drivers and keep no Sony support-mask state; OpenType and TrueTypeGx return an error with the first module left registered if the second add then fails, none register the sfnt, psaux or psnames modules their faces need, and Type42 fails driver init (`FT_Err_Missing_Module`) if truetype is absent. What the console helpers register and track is unmeasured
