# libSceNKWebKit

## add Functional

- [JSValueIsInstanceOfConstructor and class objects](../../../core/libs/prx/libSceNKWebKit/src/JscValue.cpp) (libSceNKWebKit) - not checked on a console. JavaScriptCore answers false, with no exception, for a constructor whose structure lacks `ImplementsHasInstance` ([JSValueRef.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSValueRef.cpp#L274-L294)); the port takes that to mean: not a function, not a callable proxy and not an object made from a `JSClassRef`. An object made from a class with no `hasInstance` callback and no `Symbol.hasInstance` property also answers false, as `customHasInstance` does. Still different: such an object whose prototype chain reaches `Function.prototype` has its inherited `Symbol.hasInstance` called, where JavaScriptCore skips that function and asks the class. Closes when: compared with a console module.
