# libSceNKWebKit

## add Functional

- [Termination exception](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - not checked on a console. When the execution time limit stops a script, the exception handed to the caller of the API is the string `JavaScript execution terminated.`, the value [`VM::ensureTerminationException`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/VM.cpp) gives its exception. Closes when: compared with a console.
