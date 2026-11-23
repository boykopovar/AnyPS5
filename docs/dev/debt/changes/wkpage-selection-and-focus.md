# libSceNKWebKit

## add Silent stubs

- [WKPage selection and focus](../../../core/libs/prx/libSceNKWebKit/src/PageSelection.cpp) (libSceNKWebKit) - `WKPageBlurFocusedNode`, `WKPageCenterSelectionInVisibleArea`, `WKPageClearSelection`, `WKPageSetCaretVisible` and `WKPageSetMaintainsInactiveSelection` record the request (or the flag value) on the host page in memory and nothing reads it, following [WKPage.cpp](https://github.com/WebKit/WebKit/blob/26b3c24e92fec44326a54753af61a511e737d34d/Source/WebKit/UIProcess/API/C/WKPage.cpp); DOM selection, caret and focus effects, Web process synchronization and compositor integration are unmodeled.

## remove vAi26oOxOMw

## remove Cc6pT9g-ZIk

## remove Wl0OUqhLPnk

## remove 9BHilYtCbmo

## remove Iqpp9WOrXeE
