# libSceNKWebKit

## add Functional

- [WKBundleNodeHandle input element and text area state operations](../../../core/libs/prx/libSceNKWebKit/src/BundleNodeHandle.cpp) (libSceNKWebKit) - implements `WKBundleNodeHandleSetHTMLInputElementAutoFilled`, `WKBundleNodeHandleGetHTMLInputElementAutoFilled`, `WKBundleNodeHandleSetHTMLInputElementAutofilled`, `WKBundleNodeHandleGetHTMLInputElementAutofilled`, `WKBundleNodeHandleSetHTMLInputElementAutoFilledAndViewable`, `WKBundleNodeHandleSetHTMLInputElementSpellcheckEnabled`, `WKBundleNodeHandleSetHTMLInputElementValueForUser`, `WKBundleNodeHandleGetHTMLInputElementLastChangeWasUserEdit`, and `WKBundleNodeHandleGetHTMLTextAreaElementLastChangeWasUserEdit` on `BundleNodeHandle`, tracking autofill flags, spellcheck configuration, user edit origin, and input value with typed reference validation. Replaces stubs from `ConstantExports.cpp` and `Export.cpp`.
