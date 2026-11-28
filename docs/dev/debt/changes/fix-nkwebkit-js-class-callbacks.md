# libSceNKWebKit

## add Functional

- [JSClass callbacks](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. They follow the pinned [JSCallbackObjectFunctions.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSCallbackObjectFunctions.h#L143-L260): a `convertToType` that declines or returns an object ends in the ordinary `toString`/`valueOf` conversion, a static value is present only if a getter returns a value, `instanceof` an object of a class without `hasInstance` is false, and a class `callAsFunction` that returns NULL gives `null`. Gaps: `instanceof` works through an own `Symbol.hasInstance` function that JavaScriptCore objects do not have; after an enumeration, the name of a static value whose getter declines answers present until the next enumeration; a class `callAsConstructor` that returns NULL gives `undefined`
