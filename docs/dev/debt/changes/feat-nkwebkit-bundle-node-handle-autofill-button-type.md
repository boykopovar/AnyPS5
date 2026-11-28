# libSceNKWebKit

## add Functional

- [WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType, WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType, and WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType](../../../core/libs/prx/libSceNKWebKit/src/BundleNodeHandle.cpp) (libSceNKWebKit) - console execution is unmeasured; follows WebKit `e12a212a531a` `WKBundleNodeHandlePrivate.h` and `HTMLInputElement::setShowAutoFillButton`. Returns early when button type is unchanged; otherwise updates last type to previous current type before storing new type (with None 0 disabling). Null or mismatched references throw `std::invalid_argument`.

## remove | `WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType` |
## remove | `WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType` |
## remove | `WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType` |
