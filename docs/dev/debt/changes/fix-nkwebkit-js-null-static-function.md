# libSceNKWebKit

## add Functional

- [Static function with a NULL callback](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. Reading it raises the `ReferenceError` of [`staticFunctionGetterImpl`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSCallbackObjectFunctions.h#L680-L710), unless a parent class lists the name with a callback; `in`, `for...in` and `Object.keys` do not raise. `Object.getOwnPropertyDescriptor` returns an accessor descriptor where JavaScriptCore raises the error: QuickJS asks the same hook for a descriptor while it enumerates. Closes when: the hook can tell an enumeration from a read of the descriptor.
