# libSceNKWebKit

## add Functional

- [WKPage rubber banding controls](../../../core/libs/prx/libSceNKWebKit/src/PageRubberBanding.cpp) (libSceNKWebKit) - `WKPageHorizontalRubberBandingIsEnabled`, `WKPageSetEnableHorizontalRubberBanding`, `WKPageVerticalRubberBandingIsEnabled`, `WKPageSetEnableVerticalRubberBanding`, `WKPageRubberBandsAtBottom`, `WKPageSetRubberBandsAtBottom`, `WKPageRubberBandsAtLeft`, `WKPageSetRubberBandsAtLeft`, `WKPageRubberBandsAtRight`, `WKPageSetRubberBandsAtRight`, `WKPageRubberBandsAtTop` and `WKPageSetRubberBandsAtTop` configure and query directional rubber banding settings on the page in host memory. Upstream [WKPage.cpp:745-803](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/WKPage.cpp#L745-L803) setters invoke `CRASH_IF_SUSPENDED` and forward vertical/horizontal values to WebProcess; neither IPC forwarding nor suspension checks are modeled here.
