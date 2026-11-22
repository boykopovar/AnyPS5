# libSceNKWebKit

## add Functional

- [WKFrameCanShowMIMEType](../../../core/libs/prx/libSceNKWebKit/src/Frame.cpp) (libSceNKWebKit) - `WKFrameCanShowMIMEType` checks if the specified MIME type can be displayed in a frame, querying `WebCore::MIMETypeRegistry::canShowMIMEType` with type validation and null-checking, throwing `std::invalid_argument` on null or invalid references. Unverified on console.
