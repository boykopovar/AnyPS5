# libSceNKWebKit

## add Silent stubs

- [WKPageGetAllowsRemoteInspection, WKPageSetAllowsRemoteInspection](../../../core/libs/prx/libSceNKWebKit/src/PageState.cpp) (libSceNKWebKit) - store and return a remote inspection flag on the page; nothing reads it and no inspector server is started.
- [WKPageCopyPendingAPIRequestURL, WKPageCopyPageConfiguration](../../../core/libs/prx/libSceNKWebKit/src/PageState.cpp) (libSceNKWebKit) - copy the pending request URL or page configuration stored on the page; nothing sets either yet, so they return null.
