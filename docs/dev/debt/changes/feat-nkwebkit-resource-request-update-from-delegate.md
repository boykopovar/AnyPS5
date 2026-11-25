# libSceNKWebKit

## add Functional

- [WebCore::ResourceRequest::updateFromDelegatePreservingOldProperties](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) (libSceNKWebKit) - the layout in [ResourceRequestAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequestAbi.hpp) shared with `ResourceRequestBase`, together with the re-created header map, fallback array and body reference, is not checked against the PS5 module; a request with either update flag false throws because the update hooks are unknown. As [ResourceRequestCurl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/ResourceRequestCurl.cpp), it keeps the old priority, body, hidden flag, requester, initiator identifier and optional inspector node identifier across the assignment, restoring the identifier only when the old optional held a value. Tests cover the copied and preserved fields and reference counts. Closes when: compared with a console module.

## remove `_ZN7WebCore15ResourceRequest41updateFromDelegatePreservingOldPropertiesERKS0_`
