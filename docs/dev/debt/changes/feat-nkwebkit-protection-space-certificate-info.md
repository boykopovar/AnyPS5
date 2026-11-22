# libSceNKWebKit

## add Functional

- [WKProtectionSpaceCopyCertificateInfo](../../../core/libs/prx/libSceNKWebKit/src/ProtectionSpace.cpp) (libSceNKWebKit) - `WKProtectionSpaceCopyCertificateInfo` returns a retained copy of the certificate info associated with the protection space or `nullptr` if none is set, throwing `std::invalid_argument` for null or invalid references, completing all `WKProtectionSpace` accessors in `libSceNKWebKit`. Unverified on console.
