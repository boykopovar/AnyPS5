# libSceNKWebKit

## add Functional

- [Fixed and sticky viewport constraint layer positions](../../../core/libs/prx/libSceNKWebKit/src/ScrollingConstraints.cpp) (libSceNKWebKit) - not compared with a console module. `FixedPositionViewportConstraints::layerPositionForViewportRect` and `StickyPositionViewportConstraints::layerPositionForConstrainingRect` follow the pinned [ScrollingConstraints.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/scrolling/ScrollingConstraints.cpp#L39-L107) on the 48- and 104-byte layouts of [ScrollingConstraints.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/scrolling/ScrollingConstraints.h), with 8-byte `FloatSize`; the `FloatPoint` result is returned in registers. A null object or rect throws. Closes when: compared with a console module.

## remove | `_ZNK7WebCore32FixedPositionViewportConstraints28layerPositionForViewportRectERKNS_9FloatRectE` | `BEErbU1FwoI` |

## remove | `_ZNK7WebCore33StickyPositionViewportConstraints32layerPositionForConstrainingRectERKNS_9FloatRectE` | `RjbmUrQcU28` |
