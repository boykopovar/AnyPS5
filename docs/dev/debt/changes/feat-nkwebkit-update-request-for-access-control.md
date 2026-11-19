# libSceNKWebKit

## add Unknown function info

- [WebCore::updateRequestForAccessControl](../../../core/libs/prx/libSceNKWebKit/src/RequestAccessControl.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [CrossOriginAccessControl.cpp:75-80](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/loader/CrossOriginAccessControl.cpp#L75-L80), whose `StoredCredentialsPolicy` signature matches the export name: it removes URL credentials, allows cookies only for `StoredCredentialsPolicy::Use` and sets the `Origin` header to `SecurityOrigin::toString`. A policy value above 2 throws. A request URL that carries credentials still throws, because `ResourceRequestBase::removeCredentials` cannot yet strip them.

## remove `_ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE`
