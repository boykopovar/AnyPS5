# libSceNKWebKit

## add Unknown function info

- [InspectorFrontendClientLocal::Settings property methods](../../../core/libs/prx/libSceNKWebKit/src/InspectorFrontendClientLocal.cpp) (libSceNKWebKit) - not compared with a console module. `getProperty`, `setProperty` and `deleteProperty` follow [InspectorFrontendClientLocal.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/inspector/InspectorFrontendClientLocal.cpp#L136-L147): the base class keeps no property storage, so `getProperty` returns a null `WTF::String` for every name and the other two ignore their arguments. [InspectorFrontendClientLocal.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/inspector/InspectorFrontendClientLocal.h#L56-L64) declares the class with no data members, so the three methods never read or write the object, and a null object is accepted where upstream ignores it too. Closes when: compared with a console module.

## remove | `_ZN7WebCore28InspectorFrontendClientLocal8Settings11setPropertyERKN3WTF6StringES5_` | `I9uZ6uF29YY` |

## remove | `_ZN7WebCore28InspectorFrontendClientLocal8Settings14deletePropertyERKN3WTF6StringE` | `QfBKuqPXKZo` |
