# libSceNKWebKit

## add Functional

- [CORS request preparation](../../../core/libs/prx/libSceNKWebKit/src/CrossOriginRequest.cpp) (libSceNKWebKit) - not compared with a console module. `cleanHTTPRequestHeadersForAccessControl` and `updateRequestForAccessControl` follow the pinned [CrossOriginAccessControl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/CrossOriginAccessControl.cpp#L77-L83) ([clean, L217-L238](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/CrossOriginAccessControl.cpp#L217-L238)) on the `ResourceRequestBase` layout of `ResourceRequestAbi.hpp`; the option set and the credentials policy are read as one byte each. Like the other request accessors they throw when `m_resourceRequestUpdated` is false, and `updateRequestForAccessControl` inherits the throw of `removeCredentials` for a URL with credentials. Closes when: compared with a console module.

## remove | `_ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE` | `JC5ommFaOHo` |

## remove | `_ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE` | `PfN-ltCCZaU` |
