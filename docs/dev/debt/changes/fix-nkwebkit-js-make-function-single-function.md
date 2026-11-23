# libSceNKWebKit

## add Functional

- [JSObjectMakeFunction text](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. The name, the parameters and the body must form one function, as [`UnlinkedFunctionExecutable::fromGlobalCode`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/bytecode/UnlinkedFunctionExecutable.cpp) requires; text that closes the function early is refused with a `SyntaxError` and is not run. QuickJS cannot say where a function ends, so the check compiles the text several times instead, and the message is `Parser error` or the parameters message of JavaScriptCore, not always the one its parser gives. Closes when: compared with a console.
