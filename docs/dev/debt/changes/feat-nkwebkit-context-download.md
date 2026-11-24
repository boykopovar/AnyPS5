# libSceNKWebKit

## add Functional

- [WKContext download operations](../../../core/libs/prx/libSceNKWebKit/src/Context.cpp) (libSceNKWebKit) - `WKContextDownloadURLRequest` and `WKContextResumeDownload` initiate and resume downloads on `WKContext`, returning tracked `WKDownload` objects retaining the request, resume data, and optional download bundle path. Filesystem resumption I/O is not simulated.
