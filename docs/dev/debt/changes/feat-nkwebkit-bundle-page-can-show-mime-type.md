# libSceNKWebKit

## remove | `WKBundlePageCanShowMIMEType` | `Fux56rPNtvs` |

## add Functional

- [WKBundlePageCanShowMIMEType](../../../core/libs/prx/libSceNKWebKit/src/BundlePageMime.cpp) (libSceNKWebKit) - `WKBundlePageCanShowMIMEType` checks if the specified MIME type can be displayed in a bundle page, querying `WebCore::MIMETypeRegistry::canShowMIMEType` with type validation and null-checking, throwing `std::invalid_argument` on null or invalid references. Unverified on console.
