# libSceNKWebKit

## add Functional

- [Context released while it runs](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - not checked on a console. A context released by `JSGlobalContextRelease` while an export that runs script is on the stack is destroyed when the outermost of those exports returns, unless it was retained again. In JavaScriptCore every API call keeps the VM alive through its [`JSLockHolder`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSLock.cpp), and the released global object lives until it is collected. Here a context released from an `initialize` callback that `JSObjectMake` runs outside script is still destroyed at once. Closes when: every export that takes a context holds the group, and compared with a console.
