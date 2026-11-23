# libSceNKWebKit

## add Functional

- [WKBundleNodeHandle input element and text area state operations](../../../core/libs/prx/libSceNKWebKit/src/BundleNodeHandle.cpp) (libSceNKWebKit) - implements `WKBundleNodeHandleSetHTMLInputElementAutoFilled`, `WKBundleNodeHandleSetHTMLInputElementAutofilled`, `WKBundleNodeHandleSetHTMLInputElementAutoFilledAndViewable`, `WKBundleNodeHandleSetHTMLInputElementSpellcheckEnabled`, `WKBundleNodeHandleSetHTMLInputElementValueForUser`, `WKBundleNodeHandleGetHTMLInputElementLastChangeWasUserEdit`, and `WKBundleNodeHandleGetHTMLTextAreaElementLastChangeWasUserEdit` on `BundleNodeHandle`, tracking autofill flags, spellcheck configuration, user edit origin, and input value with typed reference validation. Replaces stubs from `Export.cpp`. The `AutoFilled` and `Autofilled` getters stay constant-return exports answering 0, as the 11.00 module does and as WebKit's `ASSERT_NOT_REACHED(); return false;` bodies, so the stored autofill flag is not readable.
