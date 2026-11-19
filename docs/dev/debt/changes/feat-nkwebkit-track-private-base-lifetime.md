# libSceNKWebKit

## change [WebCore::TrackPrivateBase]

- [WebCore::TrackPrivateBase](../../../core/libs/prx/libSceNKWebKit/src/TrackPrivateBase.cpp) (libSceNKWebKit) - not compared with a console module: the base constructor stores a vtable of this library and a reference count of 1 at offset 8, and leaves offsets 12 to 15 to the subclass; this assumes the release layout without logging fields, which the bare `ret` of the 11.00 base destructor supports. The vtable lists the exported virtuals in declaration order, with no type info; a subclass constructor overwrites it. `id`, `label`, `language` and `startTimeVariance` return the defaults of [TrackPrivateBase.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/TrackPrivateBase.h#L53-L72); the deleting destructor frees through `WTF::fastFree` (fast-allocated), and `willBeRemoved` calls vtable slot 5 (`willRemove`) of a non-null `client()`. A null object throws. Closes when: compared with a console module.

## remove `_ZN7WebCore16TrackPrivateBase13willBeRemovedEv`

## remove `_ZN7WebCore16TrackPrivateBaseD0Ev`

## remove `_ZN7WebCore16TrackPrivateBaseD1Ev`
