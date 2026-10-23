# libSceNpUtility technical debt

### Functional

- [NpLookup and NpWordFilter](../../../core/libs/prx/libSceNpUtility/Export.cpp) (libSceNpUtility) - offline only, not measured on a PS5: the argument checks and their order, the limit of 32 title contexts and 32 requests, the word filter id bases `0x01000001` and `0x02000001`, the 0x400-byte comment limit and single-use requests (`INVALID_TYPE`) follow shadPS4's [lookup](https://github.com/shadps4-emu/shadPS4/blob/91e6afa69b0a64b0afaa0281b80f8fbb0cd88136/src/core/libraries/np/np_utility/np_utility.cpp#L41-L222) and [word filter](https://github.com/shadps4-emu/shadPS4/blob/91e6afa69b0a64b0afaa0281b80f8fbb0cd88136/src/core/libraries/np/np_utility/np_utility.cpp#L705-L909) calls. `sceNpLookupNpId`, `sceNpWordFilterCensorComment` and `sceNpWordFilterSanitizeComment` then finish with `SCE_NP_ERROR_SIGNED_OUT` and write no NpId or comment; shadPS4's word filter accepts every comment as a stand-in for the server, so its result is not used
