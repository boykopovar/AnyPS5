# libSceJsc

## add Unknown function info

- [JSStringGetMaximumUTF8CStringSize](../../../core/libs/prx/libSceJsc/Export.cpp) (libSceJsc) - the console result is unmeasured. It keeps the 32-bit arithmetic of pinned [JSStringRef.cpp](https://github.com/WebKit/WebKit/blob/f274ef4c03ebace644c46f8ec55746b391acdcc8/Source/JavaScriptCore/API/JSStringRef.cpp#L83-L87), `return string->length() * 3 + 1`, where `OpaqueJSString::length()` returns `unsigned`. The product therefore wraps for UTF-16 lengths of 1431655766 units and up, before it widens to `size_t`. A test for that range needs about 2.9 GB, so none is kept. Closes when: measured on a PS5 or compared with a console module.
