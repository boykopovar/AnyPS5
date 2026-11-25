# libSceNKWebKit

## add Functional

- [WKContextConfiguration override languages](../../../core/libs/prx/libSceNKWebKit/src/ContextConfiguration.cpp) (libSceNKWebKit) - not compared with a console module. As in [WKContextConfigurationRef.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/WKContextConfigurationRef.cpp#L148-L160), `WKContextConfigurationCopyOverrideLanguages` always returns a new empty array, and `WKContextConfigurationSetOverrideLanguages` ignores its configuration and keeps the strings of its array as one list for the process. Handing that list to a web process when it starts, where it becomes the `WTF::overrideUserPreferredLanguages` list, is not modelled, so nothing reads the list yet. Closes when: web process start reads the list.
