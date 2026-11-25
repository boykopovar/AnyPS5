# libSceNKWebKit

## add Functional

- [Symbol.toPrimitive of class objects](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. An object of a class with `convertToType`, and a global object of such a class, gets its own `[Symbol.toPrimitive]` function when it is created, as in [`JSCallbackObject::init`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSCallbackObjectFunctions.h#L111-L131); the function is [`customToPrimitive`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSCallbackObjectFunctions.h#L228-L260). The prototype of a global object with a class is an object of the port that JavaScriptCore does not have; the function refuses it as `this`. Closes when: compared with a console.
