# libSceVnaWebsocket

## change 18 entries present in both
- [Additional callable exports](../../../core/libs/prx/libSceVnaWebsocket/Export.cpp) - 14 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unknown names use literal NID aliases. One data export is outside this callable inventory and requires separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

## remove | `Unknown` | `K-logS9LtFw` |

## remove | `Unknown` | `MI6xIf-bHXs` |

## remove | `Unknown` | `OUogBgTPp6E` |

## remove | `Unknown` | `qUkQKyPORE8` |

## add Unknown function info
- [`MI6xIf-bHXs`, `K-logS9LtFw`, `qUkQKyPORE8` and `OUogBgTPp6E`](../../../core/libs/prx/libSceVnaWebsocket/Export.cpp) (libSceVnaWebsocket) - names and meanings are unknown; read from firmware 13.60 disassembly (`0x8e90`, `0x5c0`, `0x610`). `MI6xIf-bHXs`/`K-logS9LtFw` initialize a 44-byte config struct while leaving `[0x09..0x0f]` untouched; `qUkQKyPORE8`/`OUogBgTPp6E` zero seven 64-bit slots at `0x38..0x1b8` stride `0x40`. Null or unmapped pointers throw.
