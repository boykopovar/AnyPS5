# libSceNKWebKit

## add Functional

- [WebCore::ResourceRequestBase::clearHTTPOrigin](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) (libSceNKWebKit) - not checked against a PS5 module, and it throws when `m_resourceRequestUpdated` is false, as the `WebCore::ResourceRequestBase` entry describes. It follows the pinned [ResourceRequestBase.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/ResourceRequestBase.cpp): it removes `Origin` and marks `m_platformRequestUpdated` false even when the header was absent.

## remove | `_ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv` | `OrPeJ6qQgw4` |
