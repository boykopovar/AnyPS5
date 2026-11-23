# libSceJsc

## add Functional

- [JSWeakObjectMapCreate](../../../core/libs/prx/libSceJsc/src/ApiBridge.cpp) (libSceJsc) - not compared with a console module; the JavaScriptCore version and the prototype are unverified, so both follow [JSWeakObjectMapRefPrivate.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94/Source/JavaScriptCore/API/JSWeakObjectMapRefPrivate.h) and [.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94/Source/JavaScriptCore/API/JSWeakObjectMapRefPrivate.cpp): the map belongs to the context's group and is registered there, which drops an entry when its object is collected. Contexts exist only in libSceNKWebKit; without it a call throws `std::logic_error`, and a null context `std::invalid_argument`. The destructor callback runs once, when the group is destroyed. Still different: a null destructor crashes in WebKit and is accepted here, and WebKit registers the map with its global object, not the group. Closes when: compared with a console module.

## change [JSValueMakeString and JSObjectHasPropertyForKey](../../../core/libs/prx/libSceJsc/src/ApiBridge.cpp)

- [JSValueMakeString and JSObjectHasPropertyForKey](../../../core/libs/prx/libSceJsc/src/ApiBridge.cpp) (libSceJsc) - not compared with a console module. libSceJsc exports them, but contexts and values exist only in libSceNKWebKit, which registers its QuickJS implementations at load; without it, a call with a context throws `std::logic_error`. They follow [JSValueRef.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/API/JSValueRef.cpp#L336-L347) and [JSObjectRef.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/API/JSObjectRef.cpp#L386-L406): a null context gives null or false, a null string makes the empty string, the key goes through property-key conversion and the lookup follows prototypes and proxy traps, and a thrown exception goes to `exception` and gives false. A null object or key gives false, as in the `ForKey` siblings. Closes when: compared with a console module.

# libSceNKWebKit

## add Functional

- [JSWeakObjectMapCreate and map destruction](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - not checked on a console. `CreateWeakObjectMap` registers the map with the caller's group so its finalizer can drop entries whose objects are collected. Each live context retains its group, so releasing the creating context and the caller's group reference leaves the map usable from another live context in the group. The group destructor invokes each map's destructor callback once. The pinned [WebKit implementation](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSGlobalObjectInlines.h#L393-L397) takes a map reference through `JSGlobalObject::registerWeakMap`. The console's destruction timing remains unverified.
