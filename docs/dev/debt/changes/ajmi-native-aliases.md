# libSceAjmi

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - 17 of the 20 entries shared by the version-labelled images remain throwing import-resolution declarations. Their parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders. The ATRAC9 parser, MP3 parser and error-string aliases are covered below. Neither image label independently authenticates retail firmware provenance. Data exports require separate layout and ownership analysis. Candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

## add Functional

- Console behavior remains unverified for [sceAjmiDecAt9ParseConfigData, sceAjmiDecMp3ParseFrame and sceAjmiStrError](../../../core/libs/prx/libSceAjmi/Export.cpp), which forward to the corresponding `libSceAjm.native` implementations. Their alias identity follows the firmware 13.42 symbol-table report in [#15495](https://github.com/boykopovar/AnyPS5/pull/15495): each `sceAjmi*` export shares an address with its `sceAjm*` twin in `libSceAjmi.sprx` (SHA-256 `ff0dc985c1f7a6b25a766904e3a535b55e44d0d4b8872329d2bd6c019491df0b`). Signatures and behavior inherit the native implementations and their [known gaps](libSceAjm.native.md), including the unverified PS5 MP3 and original-file-length layouts. Tests compare parser records, invalid inputs, bounds and strings through named and patched exports. No console measurement or title execution verifies these aliases; PPSA01341 imports none of them.

## remove | `sceAjmiStrError` |

## remove | `sceAjmiDecAt9ParseConfigData` |

## remove | `sceAjmiDecMp3ParseFrame` |
