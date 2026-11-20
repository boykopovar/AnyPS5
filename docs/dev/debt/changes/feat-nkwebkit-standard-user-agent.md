# libSceNKWebKit

## add Functional

- [standardUserAgent and standardUserAgentForURL](../../../core/libs/prx/libSceNKWebKit/src/WebCoreUserAgent.cpp) (libSceNKWebKit) - not compared with a console module. Both follow the pinned [UserAgentPlayStation.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/platform/playstation/UserAgentPlayStation.cpp#L42-L133) built without `HAS_GETENV_NP`, so the platform part is always `PlayStation/0.00`; a console build that reads `SYSTEM_SOFTWARE_NAME` and `SYSTEM_SOFTWARE_VERSION` through `getenv_np` would differ. The PS5 11.x [UserAgentPlayStation.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/playstation/UserAgentPlayStation.cpp) says `Version/17.0` where the pinned revision says `Version/14.0`. The static string is created once and never freed; null `String` pointers throw instead of faulting. Closes when: compared with a console module.

## remove `_ZN7WebCore17standardUserAgentERKN3WTF6StringES3_`

## remove `_ZN7WebCore23standardUserAgentForURLERKN3WTF3URLE`
