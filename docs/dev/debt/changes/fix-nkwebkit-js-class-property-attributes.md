# libSceNKWebKit

## add Functional

- [Attributes of class-supplied properties](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. A property answered by `getProperty`, `hasProperty` or a static value is described as read-only and configurable, the `ReadOnly | DontEnum` of [`getOwnPropertySlot`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSCallbackObjectFunctions.h#L154). JavaScriptCore also describes it as not enumerable and still lists it in `for...in` (its `GetOwnPropertySlotMayBeWrongAboutDontEnum` flag); QuickJS filters `for...in` by the descriptor, so the port says enumerable for the listed ones. A static function is still described by the attributes of its entry. Closes when: the descriptor and the enumeration can differ.
