# libSceNpPartner001

## remove [NpPartner001 export `pQfYTZHznMc`]

## add Functional

- [sceNpHasEAAccessSubscriptionAbortRequest](../../../core/libs/prx/libSceNpPartner001/Export.cpp) (libSceNpPartner001) - NID `pQfYTZHznMc` is `tools/nid_names.py` `compute_nid` of this name, the one [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/a26cac679b3f95fee5b591689adec9e53b62adfb/src/core/libraries/np/np_partner.cpp#L27-L34) gives it; the parameterless signature is shadPS4's and was not checked against a title's call or the module. `sceNpHasEAAccessSubscription` finishes before it returns, so no request is ever in flight: the abort returns 0 and changes nothing, and a later check still writes `false`. Whether the console's abort also fails the next check is unknown. Not initialized throws where shadPS4 returns the PS4 error `0x819D0001`, whose PS5 value is unverified. Closes when: compared with the module or a title's call
