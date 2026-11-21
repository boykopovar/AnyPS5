# libSceNKWebKit

## add Functional

- [WKPage layout setters](../../../core/libs/prx/libSceNKWebKit/src/PageLayout.cpp) (libSceNKWebKit) - not compared with a console module. They follow [WKPage.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/WKPage.cpp#L676-L686) and [WebPageProxy.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/WebPageProxy.cpp#L4579-L4617): leaving fixed layout resets the fixed size, a pagination mode above 4 is ignored, a scroll pinning value above 2 is stored as 0. `WKPageSetFixedLayoutSize` truncates each component to `int`; for NaN and values outside `int`, where `static_cast<int>` is undefined, it stores -2147483648, what the x86-64 conversion instruction gives. Closes when: compared with a console module.
