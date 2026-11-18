# libSceNKWebKit

## add Functional

- [WKDownload attributes and redirect queries](../../../core/libs/prx/libSceNKWebKit/src/Download.cpp) (libSceNKWebKit) - `WKDownloadGetID`, `WKDownloadGetResumeData` and `WKDownloadCopyRedirectChain` query download identifier, resume data and redirect chains from host download instances. Unset resume data returns null; unset redirect chain returns an empty array. Unmatched or null pointers throw `std::invalid_argument`.
