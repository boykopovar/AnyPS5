# libSceNKWebKit

## add Functional

- [WebCore::ResourceRequest::updateFromDelegatePreservingOldProperties](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequestCurl.cpp) (libSceNKWebKit) - follows the curl port's [ResourceRequestCurl.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/curl/ResourceRequestCurl.cpp#L34-L53): the request takes a member-wise copy of the delegate request over the `ResourceRequestAbi.hpp` layout, then restores its own priority, body, inspector visibility, requester, initiator identifier and any inspector initiator node identifier. The copy assumes the PS5 `ResourceRequest` adds no members to `ResourceRequestBase`, as in the curl port, and is not checked against the console module. It throws when either request has `m_resourceRequestUpdated` false or the old one has `m_resourceRequestBodyUpdated` false, like the other accessors.

## remove | `_ZN7WebCore15ResourceRequest41updateFromDelegatePreservingOldPropertiesERKS0_` | `1PjHjfI8ncQ` |
