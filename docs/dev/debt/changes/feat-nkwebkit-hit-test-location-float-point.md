# libSceNKWebKit

## add Functional

- [WebCore::HitTestLocation(const FloatPoint&) constructors](../../../core/libs/prx/libSceNKWebKit/src/WebCoreHitTestLocation.cpp) (libSceNKWebKit) - not compared with a console module. They follow WebKit 73a35b43 [HitTestLocation.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/rendering/HitTestLocation.cpp#L37-L40), which delegates to the `LayoutPoint` constructor with `flooredLayoutPoint(point)`, so the transformed point is the floored point, not the argument. `LayoutUnit::fromFloatFloor` clamps through `clampTo<int>`; a NaN coordinate, whose cast is undefined there, throws, as do a null object or point. Closes when: compared with a console module.

## change 2159 of these

- 2157 of these [older callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches. Their prototypes and behavior stay unverified, and each still throws through `NotImplemented_nid_no_patch`.
