# libSceJsc

## add Functional

- [JSWeakObjectMapCreate](../../../core/libs/prx/libSceJsc/src/ApiBridge.cpp) (libSceJsc) - console JavaScriptCore version and prototype unverified. Follows [JSWeakObjectMapRefPrivate.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94/Source/JavaScriptCore/API/JSWeakObjectMapRefPrivate.h) and [.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94/Source/JavaScriptCore/API/JSWeakObjectMapRefPrivate.cpp). The context group owns the map, drops entries when their objects are collected, and runs the callback once at group destruction. A null context throws `std::invalid_argument`. With a non-null context and no registered libSceNKWebKit bridge, the call reaches `NotImplemented_nid_no_patch`. A null destructor is accepted here and crashes in WebKit, which registers the map with its global object. Closes when: compared with a console module.

# libSceNKWebKit

## add Functional

- [JSWeakObjectMapCreate and map destruction](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - not checked on a console. `CreateWeakObjectMap` registers the map with the caller's group so its finalizer can drop entries whose objects are collected. Each live context retains its group, so releasing the creating context and the caller's group reference leaves the map usable from another live context in the group. The group destructor invokes each map's destructor callback once. The pinned [WebKit implementation](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSGlobalObjectInlines.h#L393-L397) takes a map reference through `JSGlobalObject::registerWeakMap`. The console's destruction timing remains unverified.
