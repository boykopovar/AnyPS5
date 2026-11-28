# libSceNKWebKit

## add Functional

- [WKPageGetPaginationLineGridEnabled](../../../core/libs/prx/libSceNKWebKit/src/PageLayout.cpp) (libSceNKWebKit) - returns false, and `WKPageSetPaginationLineGridEnabled` does nothing, for any page reference including null: in [WKPage.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/WKPage.cpp#L900-L907) neither reads its argument. WebKit [c55364d](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebKit/UIProcess/API/C/WKPage.cpp#L785-L793), the revision the 3.20 exports are dated to, stored the flag and returned it; that behaviour is not modelled. Closes when: compared with a console module.
