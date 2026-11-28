# libSceNKWebKit

## add Silent stubs

- [WKBundle user content and style sheet location controls](../../../core/libs/prx/libSceNKWebKit/src/BundleUserContent.cpp) (libSceNKWebKit) - `WKBundleAddUserScript`, `WKBundleAddUserStyleSheet`, `WKBundleRemoveUserScript`, `WKBundleRemoveUserScripts`, `WKBundleRemoveUserStyleSheet`, `WKBundleRemoveUserStyleSheets`, `WKBundleRemoveAllUserContent` and `WKBundleSetUserStyleSheetLocation` register and clear scripts and style sheets on the bundle page group and record the user style sheet location without injecting them into a live web process.
