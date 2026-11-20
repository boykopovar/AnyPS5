# libSceNKWebKit

## add Functional

- [WebCore::WheelEventDeltaFilter getters](../../../core/libs/prx/libSceNKWebKit/src/WheelEventDeltaFilter.cpp) (libSceNKWebKit) - not compared with a console module. `isFilteringDeltas`, `filteredDelta` and `filteredVelocity` follow [WheelEventDeltaFilter.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/page/WheelEventDeltaFilter.cpp#L52-L65), whose exports match the module's (the PS5 11.x tree has no `isFilteringDeltas`); the layout (vtable, two `FloatSize` at 8 and 16, `bool` at 24) comes from [WheelEventDeltaFilter.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/page/WheelEventDeltaFilter.h) and is not measured. A null filter throws.

## remove `_ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv`

## remove `_ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv`
