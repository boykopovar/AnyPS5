# libSceNKWebKit

## remove `_ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv`

## remove `_ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv`

## add Functional

- [WebCore::WheelEventDeltaFilter getters](../../../core/libs/prx/libSceNKWebKit/src/WheelEventDeltaFilter.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. `filteredDelta`, `filteredVelocity` and the 3.20 `isFilteringDeltas` return the fields after the vtable pointer, as [WheelEventDeltaFilter.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/WheelEventDeltaFilter.cpp#L52-L65) does with the [WheelEventDeltaFilter.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/WheelEventDeltaFilter.h#L46-L49) layout. The pinned 11.x source keeps both FloatSize fields at the same offsets but drops the flag. Closes when: compared with a console module.

## change 2159 of these

- 2158 of these [older callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches. Their prototypes and behavior stay unverified, and each still throws through `NotImplemented_nid_no_patch`.
