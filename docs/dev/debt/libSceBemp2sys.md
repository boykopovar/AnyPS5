# libSceBemp2sys technical debt

### Unknown function info

- [libSceBemp2sys pending exports](../../../core/libs/prx/libSceBemp2sys/Export.cpp) - 13 named callable exports (the create, destroy and release calls of the five parser objects, apart from the two decoder releases below) are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceBemp2sys.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.
- [The parser objects' argument checks](../../../core/libs/prx/libSceBemp2sys/Export.cpp) (libSceBemp2sys) - the create calls throw, so no parser object exists here, and the 12 named calls below plus `B1nK+wb5rME` answer only their argument checks; every other path throws. The declared arguments are the ones each body tests, with the widths it tests them at; what they point to is unknown. Only the 32-bit results are claimed.
- [The argument checks read from firmware](../../../core/libs/prx/libSceBemp2sys/Export.cpp) (libSceBemp2sys) - in firmware 13.42's `libSceBemp2sys.sprx` (SHA-256 `c222550a53a5263108e5c01a64367418f152a9fe9cabae7341a91271362bae6a`), read from disassembly and not run on a console, each returns `0x81001001` before touching the object for: a null first or second argument in the packetizer (`0x5b10`), pesparser (`0x5c00`), psdecoder (`0x4eb0`), tsdecoder (`0x5e80`) and tsfilter (`0x6fe0`) init calls; a null first, second or third argument in `bemp2sys_pesparser_parse` (`0x5c90`); a 32-bit message above 1 or 0xD, a null object or a null third argument in the pesparser (`0x5c60`) and tsfilter (`0x7040`) sendmessage calls; a message of 0x13 or 0x1C or more, or a null third argument, in the psdecoder (`0x4f10`) and tsdecoder (`0x5f30`) ones; a null first or fourth argument in the two readpacket calls (`0x4f50`, `0x5f70`); a null object in `B1nK+wb5rME` (`0x5b40`).

### Functional

- [bemp2sys_logprintf_getlevel, bemp2sys_logprintf_setfunc, bemp2sys_logprintf_setlevel and bemp2sys_memmanager_set](../../../core/libs/prx/libSceBemp2sys/Export.cpp) - the wrappers at `0x5cc0`, `0x5ca0`, `0x5cb0` and `0x5c90` in the 11.00 system module `libSceBemp2sys.sprx` (SHA-256 `d66887aa1cfb464aaeaf1abf471c6dbedcf802edb4349fe6ee9d3d3f00936208`) keep a process-global log level, log function pointer and three memory-manager pointers. `setlevel` accepts 0..3 with an unsigned compare, so larger and negative values are ignored; `getlevel` starts at 0. The pointers are stored but have no consumer until the parsers are implemented. The rest of the library keeps throwing.

- [bemp2sys_psdecoder_release and bemp2sys_tsdecoder_release](../../../core/libs/prx/libSceBemp2sys/Export.cpp) (libSceBemp2sys) - their parameter lists are unverified and they keep the no-parameter declarations. In firmware 13.42 both bodies are a lone `ret`, so they return nothing and release nothing here either. Read from disassembly, not measured on a console.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceBemp2sys/Export.cpp) - 2 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`, apart from the argument check of `B1nK+wb5rME` above. Unidentified names use literal NID aliases. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `Unknown` | `1L1WwRMiys8` |
| `Unknown` | `B1nK+wb5rME` |
