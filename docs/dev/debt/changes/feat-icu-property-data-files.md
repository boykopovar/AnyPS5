# libSceIcu

## change [Binary property sets]
- [Binary property sets](../../../core/libs/prx/libSceIcu/Set.cpp) expose immutable borrowed ranges through the existing guest `USet` API, including the emoji and NFKC properties now that their data is embedded. Basic_Emoji through RGI_Emoji throw, because ICU adds emoji sequences to those sets and the guest `USet` holds only code point ranges. Ranges follow the bundled Unicode version, and console behavior is unmeasured.

## change [`u_getIntPropertyMap`]
- [`u_getIntPropertyMap`](../../../core/libs/prx/libSceIcu/Properties.cpp) forwards integer-property map lookups to bundled ICU 78.1; its NID name comes from the recovered inventory, while the prototype follows ICU's public header. Maps for the NFKC quick check, Indic positional and syllabic category and vertical orientation properties read the embedded pinned data files. The map uses Unicode 17.0 property data and has not been checked on a console.
