# libSceNKWebKit

## change [WTF host services](../../../core/libs/prx/libSceNKWebKit/src/WtfSystem.cpp)

- [WTF host services](../../../core/libs/prx/libSceNKWebKit/src/WtfSystem.cpp) (libSceNKWebKit) - `fastMalloc`, `bmalloc` and Gigacage allocations come from the host allocator, and random values come from the host, not the console; they throw when the host cannot answer. `WTFCrash` and failed assertions abort the process.

## add Functional

- [WTF::CPUTime::forCurrentThread](../../../core/libs/prx/libSceNKWebKit/src/WtfCpuTime.cpp) (libSceNKWebKit) - reads guest `clock_gettime(14)` (`CLOCK_THREAD_CPUTIME_ID`), as the PS5 11.x [CPUTimePOSIX.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/posix/CPUTimePOSIX.cpp#L48-L54) does, and throws where its `RELEASE_ASSERT` crashes on a failed read. The guest answers with the host thread's CPU time (Windows: `GetThreadTimes`), not the console's. Closes when: measured in a title process.
