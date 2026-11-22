# libSceNKWebKit

## add Functional

- [Derived classes of an API constructor](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. When a constructor made by `JSObjectMakeConstructor` is called for a derived class, the object its callback returns takes the prototype of the new target if its own prototype is the constructor's, as in [`APICallbackFunction::constructImpl`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/APICallbackFunction.h). As there, a constructor without a callback and an object whose class has `callAsConstructor` do not adopt it. Closes when: compared with a console.
