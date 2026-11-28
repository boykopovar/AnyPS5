# libSceIcu

## add Functional

- [Converter alias table queries](../../../core/libs/prx/libSceIcu/ConverterAliases.cpp) delegate `ucnv_io_countKnownConverters` and `ucnv_io_getConverterName` to the bundled ICU alias table. The count includes known converters even when their converter data is unavailable. Canonical names are borrowed from the alias table. Results depend on the available ICU data and have not been verified on a console.
