# libSceNKWebKit

## add Functional

- [JSContextGroupSetExecutionTimeLimit](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - not checked on a console. The limit follows [Watchdog.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/Watchdog.cpp#L46-L132): counted from the outermost entry into script, stopped at the exit, still set after a termination. Gaps: the deadline is wall time where WebKit also waits for CPU time; it starts when an export that can run script is entered, not when the script itself starts; a callback that returns false and sets no limit gets another cycle, as the comment in `shouldTerminate` says, but its `callbackAlreadyStartedTimer` test never passes there; a nested API call that ends by a termination was not compared with WebKit. Closes when: the limit counts CPU time and the two open points are checked on a console.
