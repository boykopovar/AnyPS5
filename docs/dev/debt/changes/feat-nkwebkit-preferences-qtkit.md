# libSceNKWebKit

## add Functional

- [WKPreferencesGetQTKitEnabled and WKPreferencesSetQTKitEnabled](../../../core/libs/prx/libSceNKWebKit/src/Preferences.cpp) (libSceNKWebKit) - `WKPreferencesGetQTKitEnabled` and `WKPreferencesSetQTKitEnabled` query and update the `QTKitEnabled` boolean preference flag backed by `NKWebKit::Preferences`, defaulting to `false` matching historical WebKit configuration, throwing `std::invalid_argument` on null or invalid references, and participating in `WKPreferencesCreateCopy`. Unverified on console.
