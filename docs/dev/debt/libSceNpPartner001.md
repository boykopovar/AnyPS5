# libSceNpPartner001 technical debt

### Additional callable exports

- [NpPartner001 export `pQfYTZHznMc`](../../../core/libs/prx/libSceNpPartner001/Export.cpp) is a function record with qualifier `M#A` in older and later version-labelled modules. Both map library ID 12 and module ID 0 to `libSceNpPartner001`. Older module words: `0x08540001`, `0x03200004`; later: `0x13590001`, `0x13600007`. The older module contains five function NIDs; sorted-LF SHA-256: `76ff771e3e2d8c523d0ffe0132db03b6e36fa58176d3bc6a433eb04ad76aa5ab`, using the [static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298). Labels do not authenticate retail provenance; the digest identifies the NID set only. Independent verification requires a matching image. The name, signature and behavior are unverified; this declaration throws.


### Functional

- [sceNpEAAccessInitialize, sceNpEAAccessTerminate](../../../core/libs/prx/libSceNpPartner001/Export.cpp) (libSceNpPartner001) - initialization marks the library initialized without contacting the server and termination clears it, as in [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/np/np_partner.cpp); whether the console fails initialization while signed out is unknown. Termination without initialization throws where shadPS4 returns the PS4 error `0x819D0001`, whose PS5 value is unverified, and a second initialization throws where shadPS4 returns 0 without checking. PPSA23566 imports both, not confirmed on its start-up path
- [sceNpHasEAAccessSubscription](../../../core/libs/prx/libSceNpPartner001/Export.cpp) (libSceNpPartner001) - `(int userId, bool* result)` from [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/np/np_partner.cpp) and PPSA34015's only call (`mov edi, r14d`; `lea rsi, [rbp-0x67]`). Writes `false` and returns 0: AnyPS5 has no EA Access server. Not initialized or a null result throw where shadPS4 returns the PS4 errors `0x819D0001` / invalid argument, whose PS5 values are unverified. Whether the console errors while signed out, and what it writes when the user has a subscription, are unknown
