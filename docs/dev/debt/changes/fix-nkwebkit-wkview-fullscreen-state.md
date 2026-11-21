# libSceNKWebKit

## change [View scale factors, drawing and fullscreen state]

- [View scale factors, drawing and fullscreen state](../../../core/libs/prx/libSceNKWebKit/src/View.cpp) (libSceNKWebKit) - WKViewGetPageScaleFactor, WKViewSetPageScaleFactor, WKViewGetViewScaleFactor, WKViewSetViewScaleFactor and WKViewSetDrawsBackground keep view scale factors and background drawing in host memory. The fullscreen flag WKViewIsFullScreen returns is set by WKViewWillEnterFullScreen and cleared by WKViewDidExitFullScreen, as in [PlayStationWebView.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/playstation/PlayStationWebView.cpp#L112-L150); WKViewDidEnterFullScreen, WKViewWillExitFullScreen and WKViewRequestExitFullScreen leave it. Their messages to the page's fullscreen manager and compositor integration are unmodeled.
