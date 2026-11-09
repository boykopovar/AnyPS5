#include "prx/libSceNKWebKit/src/Preferences.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WKPreferences check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename TException, typename TFunction>
void RequireThrows(TFunction function) {
    bool threw = false;
    try {
        function();
    } catch (const TException&) {
        threw = true;
    }
    Require(threw);
}

NKWebKit::Retained<NKWebKit::Preferences> Create() {
    return NKWebKit::Retained<NKWebKit::Preferences>::Adopt(
        NKWebKit::Cast<NKWebKit::Preferences>(WKPreferencesCreate(), __func__));
}

NKWebKit::Retained<NKWebKit::Preferences> Copy(const void* preferences) {
    return NKWebKit::Retained<NKWebKit::Preferences>::Adopt(
        NKWebKit::Cast<NKWebKit::Preferences>(WKPreferencesCreateCopy(preferences), __func__));
}

NKWebKit::Retained<NKWebKit::String> StringCopy(const void* string) {
    return NKWebKit::Retained<NKWebKit::String>::Adopt(NKWebKit::Cast<NKWebKit::String>(string, __func__));
}

struct BooleanCase {
    const char* name;
    void (APS5_VABI *set)(const void*, bool);
    bool (APS5_VABI *get)(const void*);
    bool initial;
};

const BooleanCase booleanCases[]{
    {"AVFoundationEnabled", WKPreferencesSetAVFoundationEnabled, WKPreferencesGetAVFoundationEnabled, false},
    {"AcceleratedCompositingEnabled", WKPreferencesSetAcceleratedCompositingEnabled, WKPreferencesGetAcceleratedCompositingEnabled, true},
    {"AcceleratedDrawingEnabled", WKPreferencesSetAcceleratedDrawingEnabled, WKPreferencesGetAcceleratedDrawingEnabled, true},
    {"AggressiveTileRetentionEnabled", WKPreferencesSetAggressiveTileRetentionEnabled, WKPreferencesGetAggressiveTileRetentionEnabled, false},
    {"AllowCrossOriginSubresourcesToAskForCredentials", WKPreferencesSetAllowCrossOriginSubresourcesToAskForCredentials, WKPreferencesGetAllowCrossOriginSubresourcesToAskForCredentials, false},
    {"AllowMediaContentTypesRequiringHardwareSupportAsFallback", WKPreferencesSetAllowMediaContentTypesRequiringHardwareSupportAsFallback, WKPreferencesGetAllowMediaContentTypesRequiringHardwareSupportAsFallback, false},
    {"AllowsAirPlayForMediaPlayback", WKPreferencesSetAllowsAirPlayForMediaPlayback, WKPreferencesGetAllowsAirPlayForMediaPlayback, false},
    {"AllowsPictureInPictureMediaPlayback", WKPreferencesSetAllowsPictureInPictureMediaPlayback, WKPreferencesGetAllowsPictureInPictureMediaPlayback, false},
    {"AnimatedImageAsyncDecodingEnabled", WKPreferencesSetAnimatedImageAsyncDecodingEnabled, WKPreferencesGetAnimatedImageAsyncDecodingEnabled, true},
    {"ApplePayCapabilityDisclosureAllowed", WKPreferencesSetApplePayCapabilityDisclosureAllowed, WKPreferencesGetApplePayCapabilityDisclosureAllowed, false},
    {"ApplePayEnabled", WKPreferencesSetApplePayEnabled, WKPreferencesGetApplePayEnabled, false},
    {"AsynchronousSpellCheckingEnabled", WKPreferencesSetAsynchronousSpellCheckingEnabled, WKPreferencesGetAsynchronousSpellCheckingEnabled, false},
    {"AttachmentElementEnabled", WKPreferencesSetAttachmentElementEnabled, WKPreferencesGetAttachmentElementEnabled, false},
    {"AudioPlaybackRequiresUserGesture", WKPreferencesSetAudioPlaybackRequiresUserGesture, WKPreferencesGetAudioPlaybackRequiresUserGesture, false},
    {"AuthorAndUserStylesEnabled", WKPreferencesSetAuthorAndUserStylesEnabled, WKPreferencesGetAuthorAndUserStylesEnabled, true},
    {"BackspaceKeyNavigationEnabled", WKPreferencesSetBackspaceKeyNavigationEnabled, WKPreferencesGetBackspaceKeyNavigationEnabled, true},
    {"BeaconAPIEnabled", WKPreferencesSetBeaconAPIEnabled, WKPreferencesGetBeaconAPIEnabled, true},
    {"CSSOMViewScrollingAPIEnabled", WKPreferencesSetCSSOMViewScrollingAPIEnabled, WKPreferencesGetCSSOMViewScrollingAPIEnabled, false},
    {"CanvasUsesAcceleratedDrawing", WKPreferencesSetCanvasUsesAcceleratedDrawing, WKPreferencesGetCanvasUsesAcceleratedDrawing, true},
    {"CaptureAudioInGPUProcessEnabled", WKPreferencesSetCaptureAudioInGPUProcessEnabled, WKPreferencesGetCaptureAudioInGPUProcessEnabled, false},
    {"CaptureAudioInUIProcessEnabled", WKPreferencesSetCaptureAudioInUIProcessEnabled, WKPreferencesGetCaptureAudioInUIProcessEnabled, false},
    {"CaptureVideoInGPUProcessEnabled", WKPreferencesSetCaptureVideoInGPUProcessEnabled, WKPreferencesGetCaptureVideoInGPUProcessEnabled, false},
    {"CaptureVideoInUIProcessEnabled", WKPreferencesSetCaptureVideoInUIProcessEnabled, WKPreferencesGetCaptureVideoInUIProcessEnabled, false},
    {"CaretBrowsingEnabled", WKPreferencesSetCaretBrowsingEnabled, WKPreferencesGetCaretBrowsingEnabled, false},
    {"ColorFilterEnabled", WKPreferencesSetColorFilterEnabled, WKPreferencesGetColorFilterEnabled, false},
    {"CompositingBordersVisible", WKPreferencesSetCompositingBordersVisible, WKPreferencesGetCompositingBordersVisible, false},
    {"CompositingRepaintCountersVisible", WKPreferencesSetCompositingRepaintCountersVisible, WKPreferencesGetCompositingRepaintCountersVisible, false},
    {"ContentChangeObserverEnabled", WKPreferencesSetContentChangeObserverEnabled, WKPreferencesGetContentChangeObserverEnabled, false},
    {"CookieEnabled", WKPreferencesSetCookieEnabled, WKPreferencesGetCookieEnabled, true},
    {"CustomPasteboardDataEnabled", WKPreferencesSetCustomPasteboardDataEnabled, WKPreferencesGetCustomPasteboardDataEnabled, false},
    {"DNSPrefetchingEnabled", WKPreferencesSetDNSPrefetchingEnabled, WKPreferencesGetDNSPrefetchingEnabled, false},
    {"DOMPasteAllowed", WKPreferencesSetDOMPasteAllowed, WKPreferencesGetDOMPasteAllowed, false},
    {"DOMTimersThrottlingEnabled", WKPreferencesSetDOMTimersThrottlingEnabled, WKPreferencesGetDOMTimersThrottlingEnabled, true},
    {"DataTransferItemsEnabled", WKPreferencesSetDataTransferItemsEnabled, WKPreferencesGetDataTransferItemsEnabled, false},
    {"DatabasesEnabled", WKPreferencesSetDatabasesEnabled, WKPreferencesGetDatabasesEnabled, true},
    {"DeveloperExtrasEnabled", WKPreferencesSetDeveloperExtrasEnabled, WKPreferencesGetDeveloperExtrasEnabled, false},
    {"DiagnosticLoggingEnabled", WKPreferencesSetDiagnosticLoggingEnabled, WKPreferencesGetDiagnosticLoggingEnabled, false},
    {"DirectoryUploadEnabled", WKPreferencesSetDirectoryUploadEnabled, WKPreferencesGetDirectoryUploadEnabled, false},
    {"DownloadAttributeEnabled", WKPreferencesSetDownloadAttributeEnabled, WKPreferencesGetDownloadAttributeEnabled, false},
    {"EnableInheritURIQueryComponent", WKPreferencesSetEnableInheritURIQueryComponent, WKPreferencesGetEnableInheritURIQueryComponent, false},
    {"EncodingDetectorEnabled", WKPreferencesSetEncodingDetectorEnabled, WKPreferencesGetEncodingDetectorEnabled, false},
    {"EnumeratingAllNetworkInterfacesEnabled", WKPreferencesSetEnumeratingAllNetworkInterfacesEnabled, WKPreferencesGetEnumeratingAllNetworkInterfacesEnabled, false},
    {"FetchAPIKeepAliveEnabled", WKPreferencesSetFetchAPIKeepAliveEnabled, WKPreferencesGetFetchAPIKeepAliveEnabled, true},
    {"FileAccessFromFileURLsAllowed", WKPreferencesSetFileAccessFromFileURLsAllowed, WKPreferencesGetFileAccessFromFileURLsAllowed, false},
    {"ForceFTPDirectoryListings", WKPreferencesSetForceFTPDirectoryListings, WKPreferencesGetForceFTPDirectoryListings, false},
    {"FrameFlatteningEnabled", WKPreferencesSetFrameFlatteningEnabled, WKPreferencesGetFrameFlatteningEnabled, false},
    {"FullScreenEnabled", WKPreferencesSetFullScreenEnabled, WKPreferencesGetFullScreenEnabled, false},
    {"GamepadsEnabled", WKPreferencesSetGamepadsEnabled, WKPreferencesGetGamepadsEnabled, false},
    {"HTTPEquivEnabled", WKPreferencesSetHTTPEquivEnabled, WKPreferencesGetHTTPEquivEnabled, true},
    {"HiddenPageCSSAnimationSuspensionEnabled", WKPreferencesSetHiddenPageCSSAnimationSuspensionEnabled, WKPreferencesGetHiddenPageCSSAnimationSuspensionEnabled, false},
    {"HiddenPageDOMTimerThrottlingAutoIncreases", WKPreferencesSetHiddenPageDOMTimerThrottlingAutoIncreases, WKPreferencesGetHiddenPageDOMTimerThrottlingAutoIncreases, false},
    {"HiddenPageDOMTimerThrottlingEnabled", WKPreferencesSetHiddenPageDOMTimerThrottlingEnabled, WKPreferencesGetHiddenPageDOMTimerThrottlingEnabled, false},
    {"HighlightAPIEnabled", WKPreferencesSetHighlightAPIEnabled, WKPreferencesGetHighlightAPIEnabled, false},
    {"HyperlinkAuditingEnabled", WKPreferencesSetHyperlinkAuditingEnabled, WKPreferencesGetHyperlinkAuditingEnabled, true},
    {"ICECandidateFilteringEnabled", WKPreferencesSetICECandidateFilteringEnabled, WKPreferencesGetICECandidateFilteringEnabled, true},
    {"ImageControlsEnabled", WKPreferencesSetImageControlsEnabled, WKPreferencesGetImageControlsEnabled, false},
    {"InlineMediaPlaybackRequiresPlaysInlineAttribute", WKPreferencesSetInlineMediaPlaybackRequiresPlaysInlineAttribute, WKPreferencesGetInlineMediaPlaybackRequiresPlaysInlineAttribute, false},
    {"InteractiveFormValidationEnabled", WKPreferencesSetInteractiveFormValidationEnabled, WKPreferencesGetInteractiveFormValidationEnabled, true},
    {"IntersectionObserverEnabled", WKPreferencesSetIntersectionObserverEnabled, WKPreferencesGetIntersectionObserverEnabled, true},
    {"InvisibleMediaAutoplayPermitted", WKPreferencesSetInvisibleMediaAutoplayPermitted, WKPreferencesGetInvisibleMediaAutoplayPermitted, true},
    {"IsAccessibilityIsolatedTreeEnabled", WKPreferencesSetIsAccessibilityIsolatedTreeEnabled, WKPreferencesGetIsAccessibilityIsolatedTreeEnabled, false},
    {"JavaScriptCanAccessClipboard", WKPreferencesSetJavaScriptCanAccessClipboard, WKPreferencesGetJavaScriptCanAccessClipboard, false},
    {"JavaScriptCanOpenWindowsAutomatically", WKPreferencesSetJavaScriptCanOpenWindowsAutomatically, WKPreferencesGetJavaScriptCanOpenWindowsAutomatically, true},
    {"JavaScriptEnabled", WKPreferencesSetJavaScriptEnabled, WKPreferencesGetJavaScriptEnabled, true},
    {"JavaScriptMarkupEnabled", WKPreferencesSetJavaScriptMarkupEnabled, WKPreferencesGetJavaScriptMarkupEnabled, true},
    {"LargeImageAsyncDecodingEnabled", WKPreferencesSetLargeImageAsyncDecodingEnabled, WKPreferencesGetLargeImageAsyncDecodingEnabled, true},
    {"LegacyEncryptedMediaAPIEnabled", WKPreferencesSetLegacyEncryptedMediaAPIEnabled, WKPreferencesGetLegacyEncryptedMediaAPIEnabled, false},
    {"LinkPreloadEnabled", WKPreferencesSetLinkPreloadEnabled, WKPreferencesGetLinkPreloadEnabled, true},
    {"LoadsImagesAutomatically", WKPreferencesSetLoadsImagesAutomatically, WKPreferencesGetLoadsImagesAutomatically, true},
    {"LocalFileContentSniffingEnabled", WKPreferencesSetLocalFileContentSniffingEnabled, WKPreferencesGetLocalFileContentSniffingEnabled, false},
    {"LocalStorageEnabled", WKPreferencesSetLocalStorageEnabled, WKPreferencesGetLocalStorageEnabled, true},
    {"LogsPageMessagesToSystemConsoleEnabled", WKPreferencesSetLogsPageMessagesToSystemConsoleEnabled, WKPreferencesGetLogsPageMessagesToSystemConsoleEnabled, false},
    {"MainContentUserGestureOverrideEnabled", WKPreferencesSetMainContentUserGestureOverrideEnabled, WKPreferencesGetMainContentUserGestureOverrideEnabled, false},
    {"MediaCapabilitiesEnabled", WKPreferencesSetMediaCapabilitiesEnabled, WKPreferencesGetMediaCapabilitiesEnabled, true},
    {"MediaCaptureRequiresSecureConnection", WKPreferencesSetMediaCaptureRequiresSecureConnection, WKPreferencesGetMediaCaptureRequiresSecureConnection, false},
    {"MediaControlsScaleWithPageZoom", WKPreferencesSetMediaControlsScaleWithPageZoom, WKPreferencesGetMediaControlsScaleWithPageZoom, true},
    {"MediaDevicesEnabled", WKPreferencesSetMediaDevicesEnabled, WKPreferencesGetMediaDevicesEnabled, false},
    {"MediaPlayable", WKPreferencesSetMediaPlayable, WKPreferencesGetMediaPlayable, true},
    {"MediaPlaybackAllowsInline", WKPreferencesSetMediaPlaybackAllowsInline, WKPreferencesGetMediaPlaybackAllowsInline, true},
    {"MediaPlaybackRequiresUserGesture", WKPreferencesSetMediaPlaybackRequiresUserGesture, WKPreferencesGetMediaPlaybackRequiresUserGesture, false},
    {"MediaPreloadingEnabled", WKPreferencesSetMediaPreloadingEnabled, WKPreferencesGetMediaPreloadingEnabled, false},
    {"MediaStreamEnabled", WKPreferencesSetMediaStreamEnabled, WKPreferencesGetMediaStreamEnabled, false},
    {"MediaUserGestureInheritsFromDocument", WKPreferencesSetMediaUserGestureInheritsFromDocument, WKPreferencesGetMediaUserGestureInheritsFromDocument, false},
    {"MenuItemElementEnabled", WKPreferencesSetMenuItemElementEnabled, WKPreferencesGetMenuItemElementEnabled, false},
    {"MetaRefreshEnabled", WKPreferencesSetMetaRefreshEnabled, WKPreferencesGetMetaRefreshEnabled, true},
    {"MockCaptureDevicesEnabled", WKPreferencesSetMockCaptureDevicesEnabled, WKPreferencesGetMockCaptureDevicesEnabled, false},
    {"MockScrollbarsEnabled", WKPreferencesSetMockScrollbarsEnabled, WKPreferencesGetMockScrollbarsEnabled, false},
    {"NeedsSiteSpecificQuirks", WKPreferencesSetNeedsSiteSpecificQuirks, WKPreferencesGetNeedsSiteSpecificQuirks, true},
    {"NeedsStorageAccessFromFileURLsQuirk", WKPreferencesSetNeedsStorageAccessFromFileURLsQuirk, WKPreferencesGetNeedsStorageAccessFromFileURLsQuirk, true},
    {"NotificationsEnabled", WKPreferencesSetNotificationsEnabled, WKPreferencesGetNotificationsEnabled, false},
    {"OfflineWebApplicationCacheEnabled", WKPreferencesSetOfflineWebApplicationCacheEnabled, WKPreferencesGetOfflineWebApplicationCacheEnabled, true},
    {"PDFPluginEnabled", WKPreferencesSetPDFPluginEnabled, WKPreferencesGetPDFPluginEnabled, false},
    {"PageCacheEnabled", WKPreferencesSetPageCacheEnabled, WKPreferencesGetPageCacheEnabled, true},
    {"PageVisibilityBasedProcessSuppressionEnabled", WKPreferencesSetPageVisibilityBasedProcessSuppressionEnabled, WKPreferencesGetPageVisibilityBasedProcessSuppressionEnabled, true},
    {"PaintTimingEnabled", WKPreferencesSetPaintTimingEnabled, WKPreferencesGetPaintTimingEnabled, true},
    {"PeerConnectionEnabled", WKPreferencesSetPeerConnectionEnabled, WKPreferencesGetPeerConnectionEnabled, false},
    {"PluginsEnabled", WKPreferencesSetPluginsEnabled, WKPreferencesGetPluginsEnabled, true},
    {"ProcessSwapOnNavigationEnabled", WKPreferencesSetProcessSwapOnNavigationEnabled, WKPreferencesGetProcessSwapOnNavigationEnabled, false},
    {"PunchOutWhiteBackgroundsInDarkMode", WKPreferencesSetPunchOutWhiteBackgroundsInDarkMode, WKPreferencesGetPunchOutWhiteBackgroundsInDarkMode, false},
    {"ReferrerPolicyAttributeEnabled", WKPreferencesSetReferrerPolicyAttributeEnabled, WKPreferencesGetReferrerPolicyAttributeEnabled, true},
    {"RemotePlaybackEnabled", WKPreferencesSetRemotePlaybackEnabled, WKPreferencesGetRemotePlaybackEnabled, false},
    {"ResourceUsageOverlayVisible", WKPreferencesSetResourceUsageOverlayVisible, WKPreferencesGetResourceUsageOverlayVisible, false},
    {"RestrictedHTTPResponseAccess", WKPreferencesSetRestrictedHTTPResponseAccess, WKPreferencesGetRestrictedHTTPResponseAccess, true},
    {"ScrollingPerformanceLoggingEnabled", WKPreferencesSetScrollingPerformanceLoggingEnabled, WKPreferencesGetScrollingPerformanceLoggingEnabled, false},
    {"SelectTrailingWhitespaceEnabled", WKPreferencesSetSelectTrailingWhitespaceEnabled, WKPreferencesGetSelectTrailingWhitespaceEnabled, false},
    {"ServerTimingEnabled", WKPreferencesSetServerTimingEnabled, WKPreferencesGetServerTimingEnabled, true},
    {"ServiceControlsEnabled", WKPreferencesSetServiceControlsEnabled, WKPreferencesGetServiceControlsEnabled, false},
    {"ShouldAllowUserInstalledFonts", WKPreferencesSetShouldAllowUserInstalledFonts, WKPreferencesGetShouldAllowUserInstalledFonts, true},
    {"ShouldConvertPositionStyleOnCopy", WKPreferencesSetShouldConvertPositionStyleOnCopy, WKPreferencesGetShouldConvertPositionStyleOnCopy, false},
    {"ShouldDisplayCaptions", WKPreferencesSetShouldDisplayCaptions, WKPreferencesGetShouldDisplayCaptions, false},
    {"ShouldDisplaySubtitles", WKPreferencesSetShouldDisplaySubtitles, WKPreferencesGetShouldDisplaySubtitles, false},
    {"ShouldDisplayTextDescriptions", WKPreferencesSetShouldDisplayTextDescriptions, WKPreferencesGetShouldDisplayTextDescriptions, false},
    {"ShouldPrintBackgrounds", WKPreferencesSetShouldPrintBackgrounds, WKPreferencesGetShouldPrintBackgrounds, false},
    {"ShouldRespectImageOrientation", WKPreferencesSetShouldRespectImageOrientation, WKPreferencesGetShouldRespectImageOrientation, false},
    {"ShouldSuppressKeyboardInputDuringProvisionalNavigation", WKPreferencesSetShouldSuppressKeyboardInputDuringProvisionalNavigation, WKPreferencesGetShouldSuppressKeyboardInputDuringProvisionalNavigation, false},
    {"ShouldUseServiceWorkerShortTimeout", WKPreferencesSetShouldUseServiceWorkerShortTimeout, WKPreferencesGetShouldUseServiceWorkerShortTimeout, false},
    {"ShowsToolTipOverTruncatedText", WKPreferencesSetShowsToolTipOverTruncatedText, WKPreferencesGetShowsToolTipOverTruncatedText, false},
    {"ShowsURLsInToolTipsEnabled", WKPreferencesSetShowsURLsInToolTipsEnabled, WKPreferencesGetShowsURLsInToolTipsEnabled, false},
    {"SmartInsertDeleteEnabled", WKPreferencesSetSmartInsertDeleteEnabled, WKPreferencesGetSmartInsertDeleteEnabled, true},
    {"SpatialNavigationEnabled", WKPreferencesSetSpatialNavigationEnabled, WKPreferencesGetSpatialNavigationEnabled, false},
    {"StorageAccessAPIEnabled", WKPreferencesSetStorageAccessAPIEnabled, WKPreferencesGetStorageAccessAPIEnabled, true},
    {"SuppressesIncrementalRendering", WKPreferencesSetSuppressesIncrementalRendering, WKPreferencesGetSuppressesIncrementalRendering, false},
    {"SyntheticEditingCommandsEnabled", WKPreferencesSetSyntheticEditingCommandsEnabled, WKPreferencesGetSyntheticEditingCommandsEnabled, true},
    {"TabsToLinks", WKPreferencesSetTabsToLinks, WKPreferencesGetTabsToLinks, false},
    {"TelephoneNumberParsingEnabled", WKPreferencesSetTelephoneNumberParsingEnabled, WKPreferencesGetTelephoneNumberParsingEnabled, false},
    {"TextAreasAreResizable", WKPreferencesSetTextAreasAreResizable, WKPreferencesGetTextAreasAreResizable, true},
    {"TextAutosizingEnabled", WKPreferencesSetTextAutosizingEnabled, WKPreferencesGetTextAutosizingEnabled, false},
    {"TextAutosizingUsesIdempotentMode", WKPreferencesSetTextAutosizingUsesIdempotentMode, WKPreferencesGetTextAutosizingUsesIdempotentMode, false},
    {"ThreadedScrollingEnabled", WKPreferencesSetThreadedScrollingEnabled, WKPreferencesGetThreadedScrollingEnabled, true},
    {"TiledScrollingIndicatorVisible", WKPreferencesSetTiledScrollingIndicatorVisible, WKPreferencesGetTiledScrollingIndicatorVisible, false},
    {"TopNavigationToDataURLsAllowed", WKPreferencesSetTopNavigationToDataURLsAllowed, WKPreferencesGetTopNavigationToDataURLsAllowed, false},
    {"UniversalAccessFromFileURLsAllowed", WKPreferencesSetUniversalAccessFromFileURLsAllowed, WKPreferencesGetUniversalAccessFromFileURLsAllowed, false},
    {"UseGiantTiles", WKPreferencesSetUseGiantTiles, WKPreferencesGetUseGiantTiles, false},
    {"VideoPlaybackRequiresUserGesture", WKPreferencesSetVideoPlaybackRequiresUserGesture, WKPreferencesGetVideoPlaybackRequiresUserGesture, false},
    {"ViewGestureDebuggingEnabled", WKPreferencesSetViewGestureDebuggingEnabled, WKPreferencesGetViewGestureDebuggingEnabled, false},
    {"WebArchiveDebugModeEnabled", WKPreferencesSetWebArchiveDebugModeEnabled, WKPreferencesGetWebArchiveDebugModeEnabled, false},
    {"WebAudioEnabled", WKPreferencesSetWebAudioEnabled, WKPreferencesGetWebAudioEnabled, false},
    {"WebAuthenticationEnabled", WKPreferencesSetWebAuthenticationEnabled, WKPreferencesGetWebAuthenticationEnabled, false},
    {"WebGLEnabled", WKPreferencesSetWebGLEnabled, WKPreferencesGetWebGLEnabled, true},
    {"WebSecurityEnabled", WKPreferencesSetWebSecurityEnabled, WKPreferencesGetWebSecurityEnabled, true},
    {"WebShareEnabled", WKPreferencesSetWebShareEnabled, WKPreferencesGetWebShareEnabled, false},
};

void Booleans() {
    for (const auto& entry : booleanCases) {
        auto preferences = Create();
        if (entry.get(preferences.Get()) != entry.initial) {
            std::fprintf(stderr, "Wrong default for %s\n", entry.name);
            Require(false);
        }
        for (bool value : {false, true, false, true}) {
            entry.set(preferences.Get(), value);
            Require(entry.get(preferences.Get()) == value);
            auto copy = Copy(preferences.Get());
            Require(entry.get(copy.Get()) == value);
            entry.set(copy.Get(), !value);
            Require(entry.get(copy.Get()) == !value);
            Require(entry.get(preferences.Get()) == value);
        }
        RequireThrows<std::invalid_argument>([&] { entry.get(nullptr); });
        RequireThrows<std::invalid_argument>([&] { entry.set(nullptr, true); });
    }
}

void Deprecated() {
    WKPreferencesSetAVFoundationNSURLSessionEnabled(nullptr, true);
    Require(WKPreferencesGetAVFoundationNSURLSessionEnabled(nullptr) == true);
    WKPreferencesSetAVFoundationNSURLSessionEnabled(nullptr, false);
    Require(WKPreferencesGetAVFoundationNSURLSessionEnabled(nullptr) == true);
    WKPreferencesSetAntialiasedFontDilationEnabled(nullptr, true);
    Require(WKPreferencesGetAntialiasedFontDilationEnabled(nullptr) == false);
    WKPreferencesSetAntialiasedFontDilationEnabled(nullptr, false);
    Require(WKPreferencesGetAntialiasedFontDilationEnabled(nullptr) == false);
    WKPreferencesSetArtificialPluginInitializationDelayEnabled(nullptr, true);
    Require(WKPreferencesGetArtificialPluginInitializationDelayEnabled(nullptr) == false);
    WKPreferencesSetArtificialPluginInitializationDelayEnabled(nullptr, false);
    Require(WKPreferencesGetArtificialPluginInitializationDelayEnabled(nullptr) == false);
    WKPreferencesSetAsynchronousPluginInitializationEnabled(nullptr, true);
    Require(WKPreferencesGetAsynchronousPluginInitializationEnabled(nullptr) == false);
    WKPreferencesSetAsynchronousPluginInitializationEnabled(nullptr, false);
    Require(WKPreferencesGetAsynchronousPluginInitializationEnabled(nullptr) == false);
    WKPreferencesSetAsynchronousPluginInitializationEnabledForAllPlugins(nullptr, true);
    Require(WKPreferencesGetAsynchronousPluginInitializationEnabledForAllPlugins(nullptr) == false);
    WKPreferencesSetAsynchronousPluginInitializationEnabledForAllPlugins(nullptr, false);
    Require(WKPreferencesGetAsynchronousPluginInitializationEnabledForAllPlugins(nullptr) == false);
    WKPreferencesSetAutostartOriginPlugInSnapshottingEnabled(nullptr, true);
    Require(WKPreferencesGetAutostartOriginPlugInSnapshottingEnabled(nullptr) == false);
    WKPreferencesSetAutostartOriginPlugInSnapshottingEnabled(nullptr, false);
    Require(WKPreferencesGetAutostartOriginPlugInSnapshottingEnabled(nullptr) == false);
    WKPreferencesSetCrossOriginResourcePolicyEnabled(nullptr, true);
    Require(WKPreferencesGetCrossOriginResourcePolicyEnabled(nullptr) == true);
    WKPreferencesSetCrossOriginResourcePolicyEnabled(nullptr, false);
    Require(WKPreferencesGetCrossOriginResourcePolicyEnabled(nullptr) == true);
    WKPreferencesSetFetchAPIEnabled(nullptr, true);
    Require(WKPreferencesGetFetchAPIEnabled(nullptr) == true);
    WKPreferencesSetFetchAPIEnabled(nullptr, false);
    Require(WKPreferencesGetFetchAPIEnabled(nullptr) == true);
    WKPreferencesSetHixie76WebSocketProtocolEnabled(nullptr, true);
    Require(WKPreferencesGetHixie76WebSocketProtocolEnabled(nullptr) == false);
    WKPreferencesSetHixie76WebSocketProtocolEnabled(nullptr, false);
    Require(WKPreferencesGetHixie76WebSocketProtocolEnabled(nullptr) == false);
    WKPreferencesSetIgnoreViewportScalingConstraints(nullptr, true);
    Require(WKPreferencesGetIgnoreViewportScalingConstraints(nullptr) == false);
    WKPreferencesSetIgnoreViewportScalingConstraints(nullptr, false);
    Require(WKPreferencesGetIgnoreViewportScalingConstraints(nullptr) == false);
    WKPreferencesSetIsSecureContextAttributeEnabled(nullptr, true);
    Require(WKPreferencesGetIsSecureContextAttributeEnabled(nullptr) == true);
    WKPreferencesSetIsSecureContextAttributeEnabled(nullptr, false);
    Require(WKPreferencesGetIsSecureContextAttributeEnabled(nullptr) == true);
    WKPreferencesSetJavaEnabled(nullptr, true);
    Require(WKPreferencesGetJavaEnabled(nullptr) == false);
    WKPreferencesSetJavaEnabled(nullptr, false);
    Require(WKPreferencesGetJavaEnabled(nullptr) == false);
    WKPreferencesSetKeygenElementEnabled(nullptr, true);
    Require(WKPreferencesGetKeygenElementEnabled(nullptr) == false);
    WKPreferencesSetKeygenElementEnabled(nullptr, false);
    Require(WKPreferencesGetKeygenElementEnabled(nullptr) == false);
    WKPreferencesSetLoadsSiteIconsIgnoringImageLoadingPreference(nullptr, true);
    Require(WKPreferencesGetLoadsSiteIconsIgnoringImageLoadingPreference(nullptr) == false);
    WKPreferencesSetLoadsSiteIconsIgnoringImageLoadingPreference(nullptr, false);
    Require(WKPreferencesGetLoadsSiteIconsIgnoringImageLoadingPreference(nullptr) == false);
    WKPreferencesSetLongMousePressEnabled(nullptr, true);
    Require(WKPreferencesGetLongMousePressEnabled(nullptr) == false);
    WKPreferencesSetLongMousePressEnabled(nullptr, false);
    Require(WKPreferencesGetLongMousePressEnabled(nullptr) == false);
    WKPreferencesSetNewBlockInsideInlineModelEnabled(nullptr, true);
    Require(WKPreferencesGetNewBlockInsideInlineModelEnabled(nullptr) == false);
    WKPreferencesSetNewBlockInsideInlineModelEnabled(nullptr, false);
    Require(WKPreferencesGetNewBlockInsideInlineModelEnabled(nullptr) == false);
    WKPreferencesSetPageCacheSupportsPlugins(nullptr, true);
    Require(WKPreferencesGetPageCacheSupportsPlugins(nullptr) == false);
    WKPreferencesSetPageCacheSupportsPlugins(nullptr, false);
    Require(WKPreferencesGetPageCacheSupportsPlugins(nullptr) == false);
    WKPreferencesSetPaginateDuringLayoutEnabled(nullptr, true);
    Require(WKPreferencesGetPaginateDuringLayoutEnabled(nullptr) == false);
    WKPreferencesSetPaginateDuringLayoutEnabled(nullptr, false);
    Require(WKPreferencesGetPaginateDuringLayoutEnabled(nullptr) == false);
    WKPreferencesSetPlugInSnapshottingEnabled(nullptr, true);
    Require(WKPreferencesGetPlugInSnapshottingEnabled(nullptr) == false);
    WKPreferencesSetPlugInSnapshottingEnabled(nullptr, false);
    Require(WKPreferencesGetPlugInSnapshottingEnabled(nullptr) == false);
    WKPreferencesSetPluginSandboxProfilesEnabledForAllPlugins(nullptr, true);
    Require(WKPreferencesGetPluginSandboxProfilesEnabledForAllPlugins(nullptr) == false);
    WKPreferencesSetPluginSandboxProfilesEnabledForAllPlugins(nullptr, false);
    Require(WKPreferencesGetPluginSandboxProfilesEnabledForAllPlugins(nullptr) == false);
    WKPreferencesSetPrimaryPlugInSnapshotDetectionEnabled(nullptr, true);
    Require(WKPreferencesGetPrimaryPlugInSnapshotDetectionEnabled(nullptr) == false);
    WKPreferencesSetPrimaryPlugInSnapshotDetectionEnabled(nullptr, false);
    Require(WKPreferencesGetPrimaryPlugInSnapshotDetectionEnabled(nullptr) == false);
    WKPreferencesSetPrivateBrowsingEnabled(nullptr, true);
    Require(WKPreferencesGetPrivateBrowsingEnabled(nullptr) == false);
    WKPreferencesSetPrivateBrowsingEnabled(nullptr, false);
    Require(WKPreferencesGetPrivateBrowsingEnabled(nullptr) == false);
    WKPreferencesSetRequestAnimationFrameEnabled(nullptr, true);
    Require(WKPreferencesGetRequestAnimationFrameEnabled(nullptr) == true);
    WKPreferencesSetRequestAnimationFrameEnabled(nullptr, false);
    Require(WKPreferencesGetRequestAnimationFrameEnabled(nullptr) == true);
    WKPreferencesSetResourceTimingEnabled(nullptr, true);
    Require(WKPreferencesGetResourceTimingEnabled(nullptr) == true);
    WKPreferencesSetResourceTimingEnabled(nullptr, false);
    Require(WKPreferencesGetResourceTimingEnabled(nullptr) == true);
    WKPreferencesSetSnapshotAllPlugIns(nullptr, true);
    Require(WKPreferencesGetSnapshotAllPlugIns(nullptr) == false);
    WKPreferencesSetSnapshotAllPlugIns(nullptr, false);
    Require(WKPreferencesGetSnapshotAllPlugIns(nullptr) == false);
    WKPreferencesSetSubpixelAntialiasedLayerTextEnabled(nullptr, true);
    Require(WKPreferencesGetSubpixelAntialiasedLayerTextEnabled(nullptr) == false);
    WKPreferencesSetSubpixelAntialiasedLayerTextEnabled(nullptr, false);
    Require(WKPreferencesGetSubpixelAntialiasedLayerTextEnabled(nullptr) == false);
    WKPreferencesSetSubpixelCSSOMElementMetricsEnabled(nullptr, true);
    Require(WKPreferencesGetSubpixelCSSOMElementMetricsEnabled(nullptr) == false);
    WKPreferencesSetSubpixelCSSOMElementMetricsEnabled(nullptr, false);
    Require(WKPreferencesGetSubpixelCSSOMElementMetricsEnabled(nullptr) == false);
    WKPreferencesSetUserTimingEnabled(nullptr, true);
    Require(WKPreferencesGetUserTimingEnabled(nullptr) == true);
    WKPreferencesSetUserTimingEnabled(nullptr, false);
    Require(WKPreferencesGetUserTimingEnabled(nullptr) == true);
    WKPreferencesSetWebSQLDisabled(nullptr, true);
    Require(WKPreferencesGetWebSQLDisabled(nullptr) == true);
    WKPreferencesSetWebSQLDisabled(nullptr, false);
    Require(WKPreferencesGetWebSQLDisabled(nullptr) == true);
    WKPreferencesSetXSSAuditorEnabled(nullptr, true);
    Require(WKPreferencesGetXSSAuditorEnabled(nullptr) == false);
    WKPreferencesSetXSSAuditorEnabled(nullptr, false);
    Require(WKPreferencesGetXSSAuditorEnabled(nullptr) == false);
}

void Numeric() {
    auto preferences = Create();
    Require(WKPreferencesGetDefaultFontSize(preferences.Get()) == 16);
    Require(WKPreferencesGetDefaultFixedFontSize(preferences.Get()) == 13);
    Require(WKPreferencesGetMinimumFontSize(preferences.Get()) == 0);
    Require(WKPreferencesGetMinimumZoomFontSize(preferences.Get()) == 0);
    Require(WKPreferencesGetIncrementalRenderingSuppressionTimeout(preferences.Get()) == 5);
    Require(WKPreferencesGetInactiveMediaCaptureSteamRepromptIntervalInMinutes(preferences.Get()) == 10);
    const auto largest = std::numeric_limits<std::uint32_t>::max();
    for (auto value : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{65537}, largest}) {
        WKPreferencesSetDefaultFontSize(preferences.Get(), value);
        WKPreferencesSetDefaultFixedFontSize(preferences.Get(), value);
        WKPreferencesSetMinimumFontSize(preferences.Get(), value);
        WKPreferencesSetJavaScriptRuntimeFlags(preferences.Get(), value);
        WKPreferencesSetVisibleDebugOverlayRegions(preferences.Get(), value);
        Require(WKPreferencesGetDefaultFontSize(preferences.Get()) == value);
        Require(WKPreferencesGetDefaultFixedFontSize(preferences.Get()) == value);
        Require(WKPreferencesGetMinimumFontSize(preferences.Get()) == value);
        Require(WKPreferencesGetJavaScriptRuntimeFlags(preferences.Get()) == value);
        Require(WKPreferencesGetVisibleDebugOverlayRegions(preferences.Get()) == value);
    }
    for (double value : {0.0, -0.0, -12.5, 0.125, 1e200, std::numeric_limits<double>::infinity()}) {
        WKPreferencesSetMinimumZoomFontSize(preferences.Get(), value);
        WKPreferencesSetIncrementalRenderingSuppressionTimeout(preferences.Get(), value);
        WKPreferencesSetInactiveMediaCaptureSteamRepromptIntervalInMinutes(preferences.Get(), value);
        Require(WKPreferencesGetMinimumZoomFontSize(preferences.Get()) == value);
        Require(WKPreferencesGetIncrementalRenderingSuppressionTimeout(preferences.Get()) == value);
        Require(WKPreferencesGetInactiveMediaCaptureSteamRepromptIntervalInMinutes(preferences.Get()) == value);
        auto copy = Copy(preferences.Get());
        Require(WKPreferencesGetIncrementalRenderingSuppressionTimeout(copy.Get()) == value);
    }
    WKPreferencesSetIncrementalRenderingSuppressionTimeout(preferences.Get(), std::numeric_limits<double>::quiet_NaN());
    Require(std::isnan(WKPreferencesGetIncrementalRenderingSuppressionTimeout(preferences.Get())));
    Require(WKPreferencesGetEditableLinkBehavior(preferences.Get()) == kWKEditableLinkBehaviorNeverLive);
    for (std::uint32_t value = 0; value <= 4; ++value) {
        WKPreferencesSetEditableLinkBehavior(preferences.Get(), static_cast<WKEditableLinkBehavior>(value));
        Require(WKPreferencesGetEditableLinkBehavior(preferences.Get()) == value);
    }
    Require(WKPreferencesGetStorageBlockingPolicy(preferences.Get()) == kWKAllowAllStorage);
    for (std::uint32_t value = 0; value <= 2; ++value) {
        WKPreferencesSetStorageBlockingPolicy(preferences.Get(), static_cast<WKStorageBlockingPolicy>(value));
        Require(WKPreferencesGetStorageBlockingPolicy(preferences.Get()) == value);
    }
    preferences->Set(u"DefaultFontSize", true);
    Require(WKPreferencesGetDefaultFontSize(preferences.Get()) == 16);
    preferences->Set(u"PluginsEnabled", std::uint32_t{7});
    Require(WKPreferencesGetPluginsEnabled(preferences.Get()));
    preferences->Set(u"StorageBlockingPolicy", true);
    Require(WKPreferencesGetStorageBlockingPolicy(preferences.Get()) == kWKAllowAllStorage);
    auto typedCopy = Copy(preferences.Get());
    Require(WKPreferencesGetPluginsEnabled(typedCopy.Get()));
    Require(WKPreferencesGetStorageBlockingPolicy(typedCopy.Get()) == kWKAllowAllStorage);
    WKPreferencesSetEditableLinkBehavior(preferences.Get(), static_cast<WKEditableLinkBehavior>(255));
    Require(WKPreferencesGetEditableLinkBehavior(preferences.Get()) == kWKEditableLinkBehaviorNeverLive);
    WKPreferencesSetStorageBlockingPolicy(preferences.Get(), static_cast<WKStorageBlockingPolicy>(255));
    Require(WKPreferencesGetStorageBlockingPolicy(preferences.Get()) == kWKAllowAllStorage);
    Require(WKPreferencesGetUserInterfaceDirectionPolicy(preferences.Get()) == kWKUserInterfaceDirectionPolicyContent);
    WKPreferencesSetUserInterfaceDirectionPolicy(preferences.Get(), kWKUserInterfaceDirectionPolicySystem);
    Require(WKPreferencesGetUserInterfaceDirectionPolicy(preferences.Get()) == kWKUserInterfaceDirectionPolicySystem);
}

struct StringCase {
    void (APS5_VABI *set)(const void*, const void*);
    const void* (APS5_VABI *copy)(const void*);
    const char16_t* initial;
};

const StringCase stringCases[]{
    {WKPreferencesSetCursiveFontFamily, WKPreferencesCopyCursiveFontFamily, u"Comic Sans MS"},
    {WKPreferencesSetDefaultTextEncodingName, WKPreferencesCopyDefaultTextEncodingName, u"ISO-8859-1"},
    {WKPreferencesSetFTPDirectoryTemplatePath, WKPreferencesCopyFTPDirectoryTemplatePath, u""},
    {WKPreferencesSetFantasyFontFamily, WKPreferencesCopyFantasyFontFamily, u"Impact"},
    {WKPreferencesSetFixedFontFamily, WKPreferencesCopyFixedFontFamily, u"Courier New"},
    {WKPreferencesSetMediaContentTypesRequiringHardwareSupport, WKPreferencesCopyMediaContentTypesRequiringHardwareSupport, u""},
    {WKPreferencesSetPictographFontFamily, WKPreferencesCopyPictographFontFamily, u"Times"},
    {WKPreferencesSetSansSerifFontFamily, WKPreferencesCopySansSerifFontFamily, u"Helvetica"},
    {WKPreferencesSetSerifFontFamily, WKPreferencesCopySerifFontFamily, u"Times"},
    {WKPreferencesSetStandardFontFamily, WKPreferencesCopyStandardFontFamily, u"Times"},
};

void Strings() {
    for (const auto& entry : stringCases) {
        auto preferences = Create();
        auto initial = StringCopy(entry.copy(preferences.Get()));
        Require(!initial->null && initial->characters == entry.initial);
        for (std::u16string value : {std::u16string(), std::u16string(u"???\U0001f3ae"), std::u16string(u"a\0b", 3), std::u16string(1, char16_t{0xd800})}) {
            auto string = NKWebKit::Make<NKWebKit::String>(value);
            entry.set(preferences.Get(), string.Get());
            string->characters = u"changed after setting";
            auto first = StringCopy(entry.copy(preferences.Get()));
            auto second = StringCopy(entry.copy(preferences.Get()));
            Require(first.Get() != second.Get());
            Require(!first->null && first->characters == value);
            Require(!second->null && second->characters == value);
            auto copy = Copy(preferences.Get());
            entry.set(preferences.Get(), nullptr);
            auto copied = StringCopy(entry.copy(copy.Get()));
            Require(!copied->null && copied->characters == value);
            auto nullValue = StringCopy(entry.copy(preferences.Get()));
            Require(!nullValue->null && nullValue->characters.empty());
            Require(first->characters == value);
        }
        auto nullString = NKWebKit::Make<NKWebKit::String>();
        entry.set(preferences.Get(), nullString.Get());
        auto nullValue = StringCopy(entry.copy(preferences.Get()));
        Require(!nullValue->null && nullValue->characters.empty());
        RequireThrows<std::invalid_argument>([&] { entry.copy(nullptr); });
        RequireThrows<std::invalid_argument>([&] { entry.set(nullptr, nullptr); });
        RequireThrows<std::invalid_argument>([&] { entry.set(preferences.Get(), preferences.Get()); });
    }
}

void Features() {
    auto preferences = Create();
    auto javascript = NKWebKit::Make<NKWebKit::String>(std::u16string(u"JavaScriptEnabled"));
    WKPreferencesSetExperimentalFeatureForKey(preferences.Get(), false, javascript.Get());
    Require(!WKPreferencesGetJavaScriptEnabled(preferences.Get()));
    WKPreferencesSetInternalDebugFeatureForKey(preferences.Get(), true, javascript.Get());
    Require(WKPreferencesGetJavaScriptEnabled(preferences.Get()));
    auto disabled = NKWebKit::Make<NKWebKit::String>(std::u16string(u"WebAudioEnabled"));
    WKPreferencesSetWebAudioEnabled(preferences.Get(), true);
    WKPreferencesSetExperimentalFeatureForKey(preferences.Get(), false, disabled.Get());
    Require(WKPreferencesGetWebAudioEnabled(preferences.Get()));
    for (const auto& key : {std::u16string(), std::u16string(u"UnknownPreference"), std::u16string(u"javascriptEnabled"), std::u16string(u"DefaultFontSize")}) {
        auto string = NKWebKit::Make<NKWebKit::String>(key);
        WKPreferencesSetInternalDebugFeatureForKey(preferences.Get(), false, string.Get());
        WKPreferencesSetExperimentalFeatureForKey(preferences.Get(), true, string.Get());
    }
    Require(WKPreferencesGetDefaultFontSize(preferences.Get()) == 16);
    Require(WKPreferencesGetJavaScriptEnabled(preferences.Get()));
    WKPreferencesSetInternalDebugFeatureForKey(preferences.Get(), false, nullptr);
    WKPreferencesSetExperimentalFeatureForKey(preferences.Get(), true, nullptr);
    WKPreferencesSetStorageAccessAPIEnabled(preferences.Get(), false);
    WKPreferencesSetHighlightAPIEnabled(preferences.Get(), false);
    WKPreferencesSetAcceleratedDrawingEnabled(preferences.Get(), false);
    WKPreferencesSetCompositingBordersVisible(preferences.Get(), true);
    WKPreferencesSetFullScreenEnabled(preferences.Get(), true);
    WKPreferencesSetDefaultFontSize(preferences.Get(), 23);
    WKPreferencesEnableAllExperimentalFeatures(preferences.Get());
    Require(WKPreferencesGetStorageAccessAPIEnabled(preferences.Get()));
    Require(WKPreferencesGetHighlightAPIEnabled(preferences.Get()));
    Require(!WKPreferencesGetAcceleratedDrawingEnabled(preferences.Get()));
    Require(WKPreferencesGetFullScreenEnabled(preferences.Get()));
    Require(WKPreferencesGetDefaultFontSize(preferences.Get()) == 23);
    WKPreferencesResetAllInternalDebugFeatures(preferences.Get());
    Require(WKPreferencesGetAcceleratedDrawingEnabled(preferences.Get()));
    Require(!WKPreferencesGetCompositingBordersVisible(preferences.Get()));
    Require(WKPreferencesGetHighlightAPIEnabled(preferences.Get()));
    Require(WKPreferencesGetFullScreenEnabled(preferences.Get()));
    WKPreferencesSetJavaScriptEnabled(preferences.Get(), false);
    WKPreferencesResetTestRunnerOverrides(preferences.Get());
    Require(!WKPreferencesGetJavaScriptEnabled(preferences.Get()));
    Require(WKPreferencesGetDefaultFontSize(preferences.Get()) == 23);
    WKPreferencesSetMetaRefreshEnabled(preferences.Get(), false);
    Require(!WKPreferencesGetHTTPEquivEnabled(preferences.Get()));
    WKPreferencesSetHTTPEquivEnabled(preferences.Get(), true);
    Require(WKPreferencesGetMetaRefreshEnabled(preferences.Get()));
    WKPreferencesSetInvisibleMediaAutoplayPermitted(preferences.Get(), false);
    Require(preferences->GetBool(u"InvisibleAutoplayNotPermitted"));
    auto original = Create();
    Require(WKPreferencesGetJavaScriptEnabled(original.Get()));
    Require(WKPreferencesGetDefaultFontSize(original.Get()) == 16);
    RequireThrows<std::invalid_argument>([] { WKPreferencesEnableAllExperimentalFeatures(nullptr); });
    RequireThrows<std::invalid_argument>([] { WKPreferencesResetAllInternalDebugFeatures(nullptr); });
    RequireThrows<std::invalid_argument>([] { WKPreferencesResetTestRunnerOverrides(nullptr); });
    RequireThrows<std::invalid_argument>([&] { WKPreferencesSetExperimentalFeatureForKey(preferences.Get(), true, preferences.Get()); });
}

void CreationAndUnknownDefaults() {
    auto identifier = NKWebKit::Make<NKWebKit::String>(std::u16string(u"shared identifier"));
    auto first = NKWebKit::Retained<NKWebKit::Preferences>::Adopt(NKWebKit::Cast<NKWebKit::Preferences>(
        WKPreferencesCreateWithIdentifier(identifier.Get()), __func__));
    auto second = NKWebKit::Retained<NKWebKit::Preferences>::Adopt(NKWebKit::Cast<NKWebKit::Preferences>(
        WKPreferencesCreateWithIdentifier(identifier.Get()), __func__));
    WKPreferencesSetJavaScriptEnabled(first.Get(), false);
    Require(WKPreferencesGetJavaScriptEnabled(second.Get()));
    Require(first->Type() == NKWebKit::ObjectType::Preferences);
    auto anonymous = NKWebKit::Retained<NKWebKit::Preferences>::Adopt(NKWebKit::Cast<NKWebKit::Preferences>(
        WKPreferencesCreateWithIdentifier(nullptr), __func__));
    Require(WKPreferencesGetJavaScriptEnabled(anonymous.Get()));
    Require(first->GetBool(u"ShouldRestrictBaseURLSchemes"));
    first->Set(u"ShouldRestrictBaseURLSchemes", false);
    auto copy = Copy(first.Get());
    Require(copy->GetBool(u"ShouldRestrictBaseURLSchemes"));
    RequireThrows<std::invalid_argument>([] { WKPreferencesCreateCopy(nullptr); });
    RequireThrows<std::invalid_argument>([&] { WKPreferencesCreateWithIdentifier(first.Get()); });
    RequireThrows<std::invalid_argument>([&] { WKPreferencesGetJavaScriptEnabled(identifier.Get()); });
    RequireThrows<std::runtime_error>([&] { WKPreferencesGetMediaSourceEnabled(second.Get()); });
    WKPreferencesSetMediaSourceEnabled(second.Get(), false);
    Require(!WKPreferencesGetMediaSourceEnabled(second.Get()));
    WKPreferencesSetMediaSourceEnabled(second.Get(), true);
    Require(WKPreferencesGetMediaSourceEnabled(second.Get()));
    auto copyMediaSourceEnabled = Copy(second.Get());
    Require(WKPreferencesGetMediaSourceEnabled(copyMediaSourceEnabled.Get()));
    RequireThrows<std::runtime_error>([&] { WKPreferencesGetEncryptedMediaAPIEnabled(second.Get()); });
    WKPreferencesSetEncryptedMediaAPIEnabled(second.Get(), false);
    Require(!WKPreferencesGetEncryptedMediaAPIEnabled(second.Get()));
    WKPreferencesSetEncryptedMediaAPIEnabled(second.Get(), true);
    Require(WKPreferencesGetEncryptedMediaAPIEnabled(second.Get()));
    auto copyEncryptedMediaAPIEnabled = Copy(second.Get());
    Require(WKPreferencesGetEncryptedMediaAPIEnabled(copyEncryptedMediaAPIEnabled.Get()));
    RequireThrows<std::runtime_error>([&] { WKPreferencesGetSourceBufferChangeTypeEnabled(second.Get()); });
    WKPreferencesSetSourceBufferChangeTypeEnabled(second.Get(), false);
    Require(!WKPreferencesGetSourceBufferChangeTypeEnabled(second.Get()));
    WKPreferencesSetSourceBufferChangeTypeEnabled(second.Get(), true);
    Require(WKPreferencesGetSourceBufferChangeTypeEnabled(second.Get()));
    auto copySourceBufferChangeTypeEnabled = Copy(second.Get());
    Require(WKPreferencesGetSourceBufferChangeTypeEnabled(copySourceBufferChangeTypeEnabled.Get()));
    auto sourceKey = NKWebKit::Make<NKWebKit::String>(std::u16string(u"SourceBufferChangeTypeEnabled"));
    RequireThrows<std::runtime_error>([&] { WKPreferencesSetExperimentalFeatureForKey(second.Get(), false, sourceKey.Get()); });
    auto mediaKey = NKWebKit::Make<NKWebKit::String>(std::u16string(u"MediaSourceEnabled"));
    WKPreferencesSetExperimentalFeatureForKey(first.Get(), false, mediaKey.Get());
    Require(!WKPreferencesGetMediaSourceEnabled(first.Get()));
    auto copiedUnknown = Copy(anonymous.Get());
    RequireThrows<std::runtime_error>([&] { WKPreferencesGetMediaSourceEnabled(copiedUnknown.Get()); });
}

}

int main() {
    Booleans();
    Deprecated();
    Numeric();
    Strings();
    Features();
    CreationAndUnknownDefaults();
    std::puts("WKPreferences: defaults, boolean round trips, deprecated APIs, numeric boundaries, Unicode/null strings, copies, features and unknown media defaults passed");
}
