# libSceNKWebKit

## add Functional

- [JSC::FuzzerAgent deleting destructor](../../../core/libs/prx/libSceNKWebKit/src/FuzzerAgent.cpp) (libSceNKWebKit) - not compared with a console module. The destructor is empty in the pinned [FuzzerAgent.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/FuzzerAgent.cpp#L31-L33), and [FuzzerAgent.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/FuzzerAgent.h#L35-L40) gives the class no members, no bases and no class allocator, so the deleting destructor only frees the object through the guest global `operator delete(void*)`; whether the module calls the sized `operator delete(void*, unsigned long)` instead is not known, and both free the same block. A null object throws. Closes when: compared with a console module.

## remove `_ZN3JSC11FuzzerAgentD0Ev`
