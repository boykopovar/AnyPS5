# libSceNKWebKit

## add Functional

- [WebCore::ResourceRequestBase::isConditional](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) - follows [ResourceRequestBase.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/ResourceRequestBase.cpp): once `m_resourceRequestUpdated` is true, it returns true when the header map contains any of `If-Match`, `If-Modified-Since`, `If-None-Match`, `If-Range` or `If-Unmodified-Since`. The request layout and behavior are not checked against a PS5 module. When `m_resourceRequestUpdated` is false, it throws through the shared update path described above.

## remove `_ZNK7WebCore19ResourceRequestBase13isConditionalEv`
