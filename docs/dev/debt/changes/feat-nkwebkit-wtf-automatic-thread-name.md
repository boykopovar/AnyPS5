# libSceNKWebKit

## add Functional

- [WTF::AutomaticThread::name](../../../core/libs/prx/libSceNKWebKit/src/WtfAutomaticThread.cpp) (libSceNKWebKit) - not compared with a console module; returns the pinned base-class name `"WTF::AutomaticThread"` without reading the object, as upstream's inline `AutomaticThread::name()` does, so a console build whose base name differs would print a different string; a null `this` throws `std::invalid_argument` where upstream would return the name. Closes when: compared with a console module.

## remove | `_ZNK3WTF15AutomaticThread4nameEv` | `r8Md8WuqsoA` |
