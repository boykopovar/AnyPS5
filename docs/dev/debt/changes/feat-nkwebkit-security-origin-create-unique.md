# libSceNKWebKit

## add Functional

- [WebCore::SecurityOrigin::createUnique](../../../core/libs/prx/libSceNKWebKit/src/SecurityOrigin.cpp) (libSceNKWebKit) - not compared with a console module. The 3.20 export follows [SecurityOrigin.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/SecurityOrigin.cpp#L173-L216), renamed `createOpaque` in the pinned [11.x SecurityOrigin.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SecurityOrigin.cpp#L159-L199): a new origin with an empty domain and `isPotentiallyTrustworthy` false. It uses the opaque-identifier layout of `create(URL)`'s opaque origins; 73a35b43 kept an empty tuple and an `m_isUnique` flag instead. The `Ref` return through a hidden pointer is unverified.

## change 2159 of these

- 2158 of these [older callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches. Their prototypes and behavior stay unverified, and each still throws through `NotImplemented_nid_no_patch`.
