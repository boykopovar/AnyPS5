# libSceNKWebKit

## add Functional

- [JSC::int52AwareSpeculationFromValue](../../../core/libs/prx/libSceNKWebKit/src/JscSpeculatedType.cpp) (libSceNKWebKit) - not compared with a console module. It follows the pinned [SpeculatedType.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/bytecode/SpeculatedType.cpp#L582-L619) for every non-cell `JSValue`. A cell needs `speculationFromCell` and the cell's structure, which libSceNKWebKit does not model, so it throws. Closes when: libSceNKWebKit creates JSC cells.
