# libSceNKWebKit

## add Unknown function info

- [WebCore::DOMException::description](../../../core/libs/prx/libSceNKWebKit/src/DOMException.cpp) (libSceNKWebKit, `9kGnP+jnLiI`) - not compared with a console module. It copies the 32-row table of the pinned [DOMException.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/DOMException.cpp#L38-L82), indexed by the [ExceptionCode](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/ExceptionCode.h) order, and returns one shared `{nullptr, nullptr, 0}` record for any code from 32 up. The 24-byte `Description` (name at 0, message at 8, legacy code at 16) assumes the pinned `ASCIILiteral` holds only a `const char*` and that the enum is passed as a 32-bit unsigned value. Closes when: compared with a console module.

- [WebCore::computeCurrentAge](../../../core/libs/prx/libSceNKWebKit/src/CacheControl.cpp) (libSceNKWebKit, `zFaau6WD5yo`) - not compared with a console module. It follows the pinned [CacheValidation.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/CacheValidation.cpp#L106-L117): the larger of the clamped apparent age and the `Age` header, plus the host wall-clock time since `responseTime`. `Date` and `Age` come from the existing `ResourceResponseBase::date` and `age` exports, which parse the headers on each call rather than reading a cached console field. Closes when: compared with a console module.

- [WebCore::HTMLMediaElement media cache directory](../../../core/libs/prx/libSceNKWebKit/src/HTMLMediaElement.cpp) (libSceNKWebKit, `eokEwsWT7zI`, `u4uYkZNxIcg`) - not compared with a console module. `setMediaCacheDirectory` and `mediaCacheDirectory` share one never-destroyed `String`, as in the pinned [HTMLMediaElement.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/html/HTMLMediaElement.cpp#L7219-L7233); assignment references the new string and releases the old one. Nothing in this module reads the directory yet, because the media cache itself is not implemented. Access is not synchronized, as in the source. Closes when: compared with a console module.

## remove `_ZN7WebCore12DOMException11descriptionENS_13ExceptionCodeE`

## remove `_ZN7WebCore16HTMLMediaElement19mediaCacheDirectoryEv`

## remove `_ZN7WebCore16HTMLMediaElement22setMediaCacheDirectoryERKN3WTF6StringE`

## remove `_ZN7WebCore17computeCurrentAgeERKNS_16ResourceResponseEN3WTF8WallTimeE`
