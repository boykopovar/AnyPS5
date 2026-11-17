# libSceNKWebKit

## add Functional

- [Bundle page group identifier queries](../../../core/libs/prx/libSceNKWebKit/src/BundlePageGroup.cpp) (libSceNKWebKit) - `WKBundlePageGroupGetTypeID` and `WKBundlePageGroupCopyIdentifier` return the runtime type ID and copy the group identifier from host page group instances. Unset identifiers return an empty string. Null pointers throw `std::invalid_argument`.
