#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
std::uint32_t APS5_VABI WKAXObjectGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::AccessibilityObject);
}

std::uint32_t APS5_VABI WKApplicationCacheManagerGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKArrayGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Array);
}

std::uint32_t APS5_VABI WKAuthenticationChallengeGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::AuthenticationChallenge);
}

std::uint32_t APS5_VABI WKAuthenticationDecisionListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::AuthenticationDecisionListener);
}

std::uint32_t APS5_VABI WKBackForwardListGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BackForwardList);
}

std::uint32_t APS5_VABI WKBackForwardListItemGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BackForwardListItem);
}

std::uint32_t APS5_VABI WKBooleanGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Boolean);
}

std::uint32_t APS5_VABI WKBundleBackForwardListGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKBundleBackForwardListItemGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKBundleDOMWindowExtensionGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BundleDOMWindowExtension);
}

std::uint32_t APS5_VABI WKBundleFrameGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BundleFrame);
}

std::uint32_t APS5_VABI WKBundleGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Bundle);
}

std::uint32_t APS5_VABI WKBundleHitTestResultGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BundleHitTestResult);
}

std::uint32_t APS5_VABI WKBundleInspectorGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BundleInspector);
}

std::uint32_t APS5_VABI WKBundleNavigationActionGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKBundleNodeHandleGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BundleNodeHandle);
}

std::uint32_t APS5_VABI WKBundlePageGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BundlePage);
}

std::uint32_t APS5_VABI WKBundlePageOverlayGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BundlePageOverlay);
}

std::uint32_t APS5_VABI WKBundleRangeHandleGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BundleRangeHandle);
}

std::uint32_t APS5_VABI WKBundleScriptWorldGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::BundleScriptWorld);
}

std::uint32_t APS5_VABI WKCertificateInfoGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKConnectionGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Connection);
}

std::uint32_t APS5_VABI WKContextGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::ProcessPool);
}

std::uint32_t APS5_VABI WKContextMenuItemGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKCookieManagerGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKCredentialGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Credential);
}

std::uint32_t APS5_VABI WKDataGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Data);
}

std::uint32_t APS5_VABI WKDictionaryGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Dictionary);
}

std::uint32_t APS5_VABI WKDoubleGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Double);
}

std::uint32_t APS5_VABI WKDownloadGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Download);
}

std::uint32_t APS5_VABI WKErrorGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Error);
}

std::uint32_t APS5_VABI WKFormSubmissionListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::FormSubmissionListener);
}

std::uint32_t APS5_VABI WKFrameGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Frame);
}

std::uint32_t APS5_VABI WKFrameHandleGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::FrameHandle);
}

std::uint32_t APS5_VABI WKFrameInfoGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::FrameInfo);
}

std::uint32_t APS5_VABI WKFramePolicyListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::FramePolicyListener);
}

std::uint32_t APS5_VABI WKGeolocationManagerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::GeolocationManager);
}

std::uint32_t APS5_VABI WKGeolocationPermissionRequestGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::GeolocationPermissionRequest);
}

std::uint32_t APS5_VABI WKGeolocationPositionGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::GeolocationPosition);
}

std::uint32_t APS5_VABI WKHTTPCookieStoreGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::HTTPCookieStore);
}

std::uint32_t APS5_VABI WKHitTestResultGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::HitTestResult);
}

std::uint32_t APS5_VABI WKIconDatabaseGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::IconDatabase);
}

std::uint32_t APS5_VABI WKImageGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Image);
}

std::uint32_t APS5_VABI WKInspectorGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Inspector);
}

std::uint32_t APS5_VABI WKKeyValueStorageManagerGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKMessageListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::MessageListener);
}

std::uint32_t APS5_VABI WKNavigationActionGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::NavigationAction);
}

std::uint32_t APS5_VABI WKNavigationDataGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::NavigationData);
}

std::uint32_t APS5_VABI WKNavigationGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Navigation);
}

std::uint32_t APS5_VABI WKNavigationResponseGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::NavigationResponse);
}

std::uint32_t APS5_VABI WKNotificationGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Notification);
}

std::uint32_t APS5_VABI WKNotificationManagerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::NotificationManager);
}

std::uint32_t APS5_VABI WKNotificationPermissionRequestGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::NotificationPermissionRequest);
}

std::uint32_t APS5_VABI WKOpenPanelParametersGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::OpenPanelParameters);
}

std::uint32_t APS5_VABI WKOpenPanelResultListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::OpenPanelResultListener);
}

std::uint32_t APS5_VABI WKPageConfigurationGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::PageConfiguration);
}

std::uint32_t APS5_VABI WKPageGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Page);
}

std::uint32_t APS5_VABI WKPageGroupGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::PageGroup);
}

std::uint32_t APS5_VABI WKPageRequestStorageAccessConfirmResultListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::RequestStorageAccessConfirmResultListener);
}

std::uint32_t APS5_VABI WKPageRunBeforeUnloadConfirmPanelResultListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::RunBeforeUnloadConfirmPanelResultListener);
}

std::uint32_t APS5_VABI WKPageRunJavaScriptAlertResultListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::RunJavaScriptAlertResultListener);
}

std::uint32_t APS5_VABI WKPageRunJavaScriptConfirmResultListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::RunJavaScriptConfirmResultListener);
}

std::uint32_t APS5_VABI WKPageRunJavaScriptPromptResultListenerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::RunJavaScriptPromptResultListener);
}

std::uint32_t APS5_VABI WKPointGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Point);
}

std::uint32_t APS5_VABI WKPopupMenuItemGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::PopupMenuItem);
}

std::uint32_t APS5_VABI WKProtectionSpaceGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::ProtectionSpace);
}

std::uint32_t APS5_VABI WKRectGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Rect);
}

void APS5_VABI WKRenderLayerGetTypeID() {}

void APS5_VABI WKRenderObjectGetTypeID() {}

std::uint32_t APS5_VABI WKResourceCacheManagerGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKSecurityOriginGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::SecurityOrigin);
}

std::uint32_t APS5_VABI WKSerializedScriptValueGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::SerializedScriptValue);
}

std::uint32_t APS5_VABI WKSessionStateGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::SessionState);
}

std::uint32_t APS5_VABI WKSizeGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::Size);
}

std::uint32_t APS5_VABI WKStringGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::String);
}

std::uint32_t APS5_VABI WKUInt64GetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::UInt64);
}

std::uint32_t APS5_VABI WKURLGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::URL);
}

std::uint32_t APS5_VABI WKURLRequestGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::URLRequest);
}

std::uint32_t APS5_VABI WKURLResponseGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::URLResponse);
}

std::uint32_t APS5_VABI WKUserContentControllerGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::UserContentController);
}

std::uint32_t APS5_VABI WKUserContentExtensionStoreGetTypeID() {
    return 0;
}

std::uint32_t APS5_VABI WKUserContentURLPatternGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::UserContentURLPattern);
}

std::uint32_t APS5_VABI WKUserMediaPermissionCheckGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::UserMediaPermissionCheck);
}

std::uint32_t APS5_VABI WKUserMediaPermissionRequestGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::UserMediaPermissionRequest);
}

std::uint32_t APS5_VABI WKUserScriptGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::UserScript);
}

std::uint32_t APS5_VABI WKViewportAttributesGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::ViewportAttributes);
}

std::uint32_t APS5_VABI WKWebsiteDataStoreConfigurationGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::WebsiteDataStoreConfiguration);
}

std::uint32_t APS5_VABI WKWebsiteDataStoreGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::WebsiteDataStore);
}

std::uint32_t APS5_VABI WKWebsitePoliciesGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::WebsitePolicies);
}

std::uint32_t APS5_VABI WKWindowFeaturesGetTypeID() {
    return NKWebKit::TypeID(NKWebKit::ObjectType::WindowFeatures);
}

}
