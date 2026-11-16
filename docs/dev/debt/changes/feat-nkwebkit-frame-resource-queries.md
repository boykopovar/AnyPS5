# libSceNKWebKit

## add Functional

- [WKFrame resource and page queries](../../../core/libs/prx/libSceNKWebKit/src/Frame.cpp) (libSceNKWebKit) - `WKFrameGetMainResourceData`, `WKFrameGetPage`, `WKFrameGetWebArchive` and `WKFrameStopLoading` query the containing page, main resource data and web archive data or stop loading from host frame instances. Unset instances return null. Null or invalid pointers throw `std::invalid_argument`.
