# libSceNKWebKit

## add Functional

- [WebCore::internalError](../../../core/libs/prx/libSceNKWebKit/src/InternalError.cpp) (libSceNKWebKit) - not compared with a console module. It follows the pinned [ResourceErrorBase.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/ResourceErrorBase.cpp#L94-L100) on the 80-byte `ResourceError` of `ResourceErrorAbi.hpp`: domain `WebKitErrorDomain`, code 300, a copy of the URL, type General, and the description from this library's `localizedString`. The two release-log lines of the source are not emitted. Closes when: compared with a console module.

## remove | `_ZN7WebCore13internalErrorERKN3WTF3URLE` | `n+bQ6u1wxSk` |
