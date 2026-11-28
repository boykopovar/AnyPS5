# libSceNKWebKit

## change [WKPage zoom factor controls]

- [WKPage zoom factor controls](../../../core/libs/prx/libSceNKWebKit/src/PageZoom.cpp) (libSceNKWebKit) - `WKPageGetPageZoomFactor`, `WKPageSetPageZoomFactor`, `WKPageGetTextZoomFactor`, `WKPageSetTextZoomFactor`, `WKPageSetPageAndTextZoomFactors` and `WKPageSupportsTextZoom` configure and query page and text zoom multipliers on the page in host memory following [WKPage.cpp:625-663](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/WKPage.cpp#L625-L663) and [WebPageProxy.cpp:4385-4436](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/WebPageProxy.cpp#L4385-L4436): `WKPageSetPageZoomFactor` ignores a factor of zero or below, and the other two setters store any value, as do both setters for a NaN. Layout viewport zoom rendering and text reflow are unmodeled.
