# libSceNKWebKit

## add Functional

- [Prototype of a global object](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. `JSObjectSetPrototype` leaves a global object unchanged, because a JavaScriptCore global object is an immutable prototype exotic object ([JSGlobalObject.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSGlobalObject.h#L573)). Script can still change it: `Object.setPrototypeOf(globalThis, o)`, `Reflect.setPrototypeOf` and the `__proto__` setter succeed where JavaScriptCore raises a `TypeError` or returns false, because QuickJS has no such flag for an ordinary object. Closes when: those three paths refuse a global object.
