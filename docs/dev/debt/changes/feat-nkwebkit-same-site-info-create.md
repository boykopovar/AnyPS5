# libSceNKWebKit

## add Functional

- [WebCore SameSiteInfo::create](../../../core/libs/prx/libSceNKWebKit/src/SameSiteInfo.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. The one-argument `SameSiteInfo::create(const ResourceRequest&)` follows WebKit 73a35b43 [SameSiteInfo.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/SameSiteInfo.cpp#L34-L37) and [`isSafeMethod`](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/HTTPParsers.cpp#L914-L922): it returns the three-byte `{isSameSite, isTopSite, isSafeHTTPMethod}` struct, built from the request's same-site and top-site exports and an ASCII case-insensitive match of the method against GET, HEAD, OPTIONS and TRACE. A null request throws. Closes when: compared with a console module.

## change 2159 of these

- 2158 of these [older callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches. Their prototypes and behavior stay unverified, and each still throws through `NotImplemented_nid_no_patch`.
