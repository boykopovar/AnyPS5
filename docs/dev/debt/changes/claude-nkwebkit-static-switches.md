# libSceNKWebKit

## add Unknown function info

- [WebCore accessibility and font switches](../../../core/libs/prx/libSceNKWebKit/src/AXObjectCache.cpp) - not compared with a console module, and no page, layout or text path reads these process-wide flags yet; `WKContextSetShouldUseFontSmoothing` and `WKContextSetAlwaysUsesComplexTextCodePath` still store per-context values only. The `AXObjectCache` switches follow the non-Mac branch of the pinned [AXObjectCache.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/accessibility/AXObjectCache.cpp#L202-L229); [`FontCascade::setCodePath`](../../../core/libs/prx/libSceNKWebKit/src/FontCascade.cpp) uses the one-byte `CodePath` of the pinned FontCascade.h:203; the legacy smoothing pair follows [FontCascade.cpp at 73a35b43be7a](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/FontCascade.cpp#L587-L598).

## remove `_ZN7WebCore11FontCascade11setCodePathENS0_8CodePathE`

## remove `_ZN7WebCore13AXObjectCache19enableAccessibilityEv`

## remove `_ZN7WebCore13AXObjectCache20disableAccessibilityEv`

## remove `_ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb`
