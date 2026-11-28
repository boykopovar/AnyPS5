# libSceNKWebKit

## add Silent stubs

- [WKPage and WKBundle messaging controls](../../../core/libs/prx/libSceNKWebKit/src/PageMessaging.cpp) (libSceNKWebKit) - `WKPagePostMessageToInjectedBundle`, `WKBundlePagePostMessage`, `WKBundlePostMessage`, `WKPageTryClose` and `WKPageRunJavaScriptInMainFrame` operate on host in-memory page/bundle state and page main-frame JS context; IPC message dispatch to separate WebKit processes remains unmodeled, the JavaScript execution context does not provide DOM `window` or `document` bindings, exceptions and serialization failures are reported as generic `WKError` instances, and `WKPageTryClose` marks the page closed without invoking unload handlers.

## remove `iHPXwHi7Qwk`

## remove `guJxsLkYX9Q`

## remove `ZAywbC2T+Nw`

## remove `DxUrwaURmnw`

## remove `BRYaC0en9CQ`
