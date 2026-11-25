# libSceNKWebKit

## add Functional

- [JSObjectMakeConstructor objects](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. They follow `JSCallbackConstructor` of the pinned [JSObjectRef.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSObjectRef.cpp#L111-L128) and `APICallbackFunction.h`: `prototype` is the class prototype or `Object.prototype`, `instanceof` is the ordinary prototype test, the class callbacks, static values, `finalize` and private data are not used, and a callback that returns NULL without an exception raises a `TypeError`. Not modelled: with `new.target` other than the constructor (`class D extends C`), WebKit gives the new object the prototype of `new.target`; here the object keeps the one the callback gave it. Called as a function, the object raises QuickJS's `TypeError`; `typeof` gives `function` where JavaScriptCore gives `object`
