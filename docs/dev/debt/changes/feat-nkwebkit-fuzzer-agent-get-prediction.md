# libSceNKWebKit

## add Functional

- [JSC::FuzzerAgent::getPrediction](../../../core/libs/prx/libSceNKWebKit/src/FuzzerAgent.cpp) (libSceNKWebKit) - not compared with a console module. The base implementation in the pinned [FuzzerAgent.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/FuzzerAgent.cpp#L35-L38), identical at [73a35b43](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/runtime/FuzzerAgent.cpp#L35-L38), ignores the agent, code block and code origin and returns the passed `SpeculatedType` (a `uint64_t`) unchanged, so the export does the same for every input. Closes when: compared with a console module.

## remove `_ZN3JSC11FuzzerAgent13getPredictionEPNS_9CodeBlockERKNS_10CodeOriginEm`
