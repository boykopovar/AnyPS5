# libSceNKWebKit

## add Functional

- [Extensibility of API objects](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. An object made by the JS API that is no longer extensible refuses a new property, in an assignment and in `Object.defineProperty`, as an ordinary object does: [`JSCallbackObject`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSCallbackObjectFunctions.h) leaves both to `JSObject` once its callbacks decline. An assignment that the prototype chain refuses raises in strict code only. Overriding a static function still stores the value, as `putDirect` does there. Closes when: compared with a console.
