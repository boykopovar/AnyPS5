# libSceNKWebKit

## add Functional

- [NULL JSValueRef in the JavaScriptCore C API](../../../core/libs/prx/libSceNKWebKit/src/JscValue.cpp) (libSceNKWebKit) - not checked on a console. A NULL `JSValueRef` is the value `null`, as `toJS(globalObject, value)` makes it in [APICast.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/APICast.h#L68-L90): in the type predicates, comparisons and conversions, in call, constructor and `JSObjectMake*` arguments, and as the value or key of the property setters and the value of `JSObjectSetPrototype`. Still different: a NULL `arguments` array with a count gives `undefined` arguments and a released handle is refused or read as `undefined`, where JavaScriptCore dereferences both; the predicates and `JSValueIsStrictEqual` answer for a live value with a NULL context, where JavaScriptCore returns false. Closes when: compared with a console module.
