# libSceNKWebKit

## remove `_ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE`

## add Functional

- [WebCore::cleanHTTPRequestHeadersForAccessControl](../../../core/libs/prx/libSceNKWebKit/src/CrossOriginRequest.cpp) (libSceNKWebKit) - not compared with a console module. It follows the pinned [CrossOriginAccessControl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/CrossOriginAccessControl.cpp#L217-L238), including the 11.x `CacheControl` keep flag and the `Sec-Fetch-*` removal that WebKit 73a35b43 lacks; the choice rests on the 11.x `HTTPHeaderName` table and `ResourceRequestBase` layout the module already uses. A content type is checked with the existing CORS-safelisted rule from `isSimpleCrossOriginAccessRequest`. Like the other `ResourceRequestBase` mutators, it throws when `m_resourceRequestUpdated` is false. Closes when: the result is compared with a console module.
