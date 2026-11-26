# libSceNKWebKit

## add Functional

- [Execution time limit](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - not checked on a console. The limit of `JSContextGroupSetExecutionTimeLimit` is counted in processor time of the running thread, as [`Watchdog::shouldTerminate`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/Watchdog.cpp) does with `CPUTime::forCurrentThread`, once the same span of wall time has passed. The processor time is the host's, through the port of that function. JavaScriptCore is woken by a timer; here the engine asks at its own polling points, so a script is stopped a little later than the limit. Closes when: compared with a console.
