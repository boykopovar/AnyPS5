# libSceNKWebKit

## add Functional

- [Objects that outlive their context](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - not checked on a console. An object of a class that another context of the group still holds keeps working after its own context is released: its callbacks receive the oldest context left in the group, and `JSObjectGetGlobalContext` returns that one. JavaScriptCore passes the original global object, which the object keeps alive ([JSObjectRef.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSObjectRef.cpp#L867-L873)). With no context left in the group the callbacks are not called until one is created. Closes when: a context lives as long as its objects.
