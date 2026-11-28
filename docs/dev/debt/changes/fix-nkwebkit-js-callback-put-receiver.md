# libSceNKWebKit

## add Functional

- [Put with another receiver](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. A write that reaches an object made from a class with a receiver that is not that object (an heir, `Reflect.set` with a receiver) skips the class and is an ordinary set, as in [`JSCallbackObject::put`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSCallbackObjectFunctions.h#L274-L275). Two gaps. A write that fails in strict-mode script raises no `TypeError`: QuickJS does not tell the callback whether the code is strict. `Reflect.set(object, index, value, receiver)` asks the class for the own property in JavaScriptCore (`ordinarySetSlow`) and not here: QuickJS gives that call and `heir[index] = value` the same arguments. Closes when: both cases are told apart.
