# libSceNKWebKit

## add Functional

- [Microtask drain](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - not checked on a console. Promise jobs run when the outermost API call that entered script returns, as in [`JSLock::willReleaseLock`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSLock.cpp), and never from a call made inside a callback. JavaScriptCore drains at the release of its lock, which every API call that takes it does; here only the exports that can run script do, which leaves the queue empty at the same points as long as nothing else queues a job. Closes when: compared with a console.
