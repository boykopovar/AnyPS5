# libSceIcu

## change [Binary property sets]
- [Binary property sets](../../../core/libs/prx/libSceIcu/Set.cpp) expose borrowed, immutable sets from the bundled Unicode data. The guest `USet` API can enumerate both the code point ranges and the UTF-16 sequences in Basic_Emoji through RGI_Emoji. Console behavior is unmeasured.

## change [`u_getIntPropertyMap`]
- [`u_getIntPropertyMap`](../../../core/libs/prx/libSceIcu/Properties.cpp) forwards integer-property map lookups to bundled ICU 78.1; its NID name comes from the recovered inventory, while the prototype follows ICU's public header. Maps for the NFKC quick check, Indic positional and syllabic category and vertical orientation properties read the embedded pinned data files. The map uses Unicode 17.0 property data and has not been checked on a console.
