# libSceNKWebKit

## add Functional

- [OpaqueJSString::tryCreate](../../../core/libs/prx/libSceNKWebKit/src/OpaqueJSString.cpp) (libSceNKWebKit) - not compared with a console module. Both overloads follow the pinned [OpaqueJSString.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/JavaScriptCore/API/OpaqueJSString.cpp#L35-L49): a null `WTF::String` gives a null `RefPtr`, any other string gives a new `OpaqueJSString` with one reference. The HLE `OpaqueJSString` is the `GuestJSString` that `JSStringRef` already uses, so the result copies the 8-bit or 16-bit characters into it and works with `JSStringRetain` and `JSStringRelease`. Closes when: compared with a console module.

## remove `_ZN14OpaqueJSString9tryCreateEON3WTF6StringE`

## remove `_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE`
