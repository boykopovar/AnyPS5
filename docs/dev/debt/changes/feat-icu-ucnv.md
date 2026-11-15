# libSceIcu

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceIcu/Export.cpp) - 40 remaining throwing functions match NIDs in the public 3.20 catalogue and both version-labelled images; neither image label authenticates retail firmware provenance. Their prototypes and behavior are unverified and retain `NotImplemented_nid_no_patch`. The six locale functions described above have been removed from this throwing inventory, `u_getIntPropertyMap` is in [Properties.cpp](../../../core/libs/prx/libSceIcu/Properties.cpp), `u_tolower`, `u_toupper`, `u_totitle` and `ublock_getCode` are in [CodePoints.cpp](../../../core/libs/prx/libSceIcu/CodePoints.cpp) and six `unorm2_*` in [Normalization.cpp](../../../core/libs/prx/libSceIcu/Normalization.cpp).

## add Functional

- [Converters](../../../core/libs/prx/libSceIcu/Converter.cpp) are unverified on a console and open only what ICU 78.1 builds without data: the UTF-8/16/32 forms, UTF-7, CESU-8, IMAP-mailbox-name, SCSU, BOCU-1, ISCII, ISO-8859-1 and US-ASCII, by their stripped canonical names. Aliases (`latin1`), table charsets, HZ, ISO-2022, LMBCS and compound text need data files this build lacks; the exports throw where ICU reports `U_FILE_ACCESS_ERROR` or `U_MISSING_RESOURCE_ERROR` (so `ucnv_getCCSID` throws for UTF-7, IMAP and ISCII), for a null (default) name and for a non-ASCII `ucnv_openU` name. Behaviour is ICU 78.1's: `UBool` results are 0 or 1. Custom error callbacks, their getters, the `ucnv_cb*` writers and the ESCAPE and SUBSTITUTE callbacks throw. Closes when: compared with the console and the converter data is available.

## remove | `ucnv_close` | `JTIHqYZFMQE` |

## remove | `ucnv_convertEx` | `lPKKDtEmEgM` |

## remove | `ucnv_fromUnicode` | `6ZIjQAWKeRs` |

## remove | `ucnv_getName` | `7pCOClUrUmM` |

## remove | `ucnv_open` | `AKenFQbXGE0` |

## remove | `ucnv_reset` | `5O8bmW45o0U` |

## remove | `ucnv_setFallback` | `iEe61PaW1pg` |

## remove | `ucnv_setSubstChars` | `Ku0Rs703XaI` |

## remove | `ucnv_toUChars` | `LvSzmTZ0Mss` |

## remove | `ucnv_toUnicode` | `O1eMKYxVlTs` |
