# libSceAjmi

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - the parameterless declarations remain throwing import-resolution placeholders, not verified ABI contracts. Parser, error-string, context, instance and batch aliases are covered below. Neither version label independently authenticates retail firmware provenance. Data exports require separate layout and ownership analysis. Candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

## add Functional

- Console behavior remains unverified for [sceAjmiDecAt9ParseConfigData, sceAjmiDecMp3ParseFrame and sceAjmiStrError](../../../core/libs/prx/libSceAjmi/Export.cpp), which forward to the corresponding `libSceAjm.native` implementations. Their alias identity follows the firmware 13.42 symbol-table report in [#15495](https://github.com/boykopovar/AnyPS5/pull/15495): each `sceAjmi*` export shares an address with its `sceAjm*` twin in `libSceAjmi.sprx` (SHA-256 `ff0dc985c1f7a6b25a766904e3a535b55e44d0d4b8872329d2bd6c019491df0b`). Signatures and behavior inherit the native implementations and their [known gaps](libSceAjm.native.md), including the unverified PS5 MP3 and original-file-length layouts. Tests compare parser records, invalid inputs, bounds and strings through named and patched exports. No console measurement or title execution verifies these aliases; PPSA01341 imports none of them.

## remove | `sceAjmiStrError` |

## remove | `sceAjmiDecAt9ParseConfigData` |

## remove | `sceAjmiDecMp3ParseFrame` |

## add Context and batch aliases

- Console behavior remains unverified for [sceAjmiInitialize, sceAjmiInstanceCreate, sceAjmiInstanceDestroy, sceAjmiBatchWait and sceAjmiBatchCancel](../../../core/libs/prx/libSceAjmi/Export.cpp), which forward to the corresponding `libSceAjm.native` exports and share their handles and state. Alias identity follows the firmware 13.42 symbol-table report in [#15495](https://github.com/boykopovar/AnyPS5/pull/15495): each shares an address with its `sceAjm*` twin in `libSceAjmi.sprx` (SHA-256 `ff0dc985c1f7a6b25a766904e3a535b55e44d0d4b8872329d2bd6c019491df0b`). No console execution verifies the aliases. REA inspection of PPSA01341 at `0x1de17e0`, `0x1de1f15`, `0x1de1e9d`, `0x1de263d` and `0x1de1aec` corroborates the native initialization, create/destroy and wait argument widths only; it does not establish their behavior or the cancellation signature.

- [Context and batch backend limits](../../../core/libs/prx/libSceAjmi/Export.cpp): PS5 behavior retains the native implementation's [known gaps](libSceAjm.native.md). Initialization ignores the reserved argument, jobs execute synchronously, cancellation does not interrupt them and waiting ignores the timeout and consumes the batch ID. The wait error output is not validated for inaccessible memory. Unsupported codec flags still throw. Context finalization remains unimplemented here. Tests establish shared state, cross-module handle ownership, output bounds, invalid IDs and unsupported 64-bit flag forwarding through named and NID-patched exports; no title execution or additional codec support is established.

## remove | `sceAjmiInitialize` |

## remove | `sceAjmiInstanceCreate` |

## remove | `sceAjmiInstanceDestroy` |

## remove | `sceAjmiBatchWait` |

## remove | `sceAjmiBatchCancel` |
