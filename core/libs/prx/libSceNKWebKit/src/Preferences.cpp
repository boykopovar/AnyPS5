#include "prx/libSceNKWebKit/src/Preferences.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace NKWebKit {
namespace {

enum class FeatureKind { None, Boolean, Testable, Internal, Unverified };

struct Definition {
    Preferences::Value value;
    FeatureKind feature;
};

const std::map<std::u16string_view, Definition, std::less<>>& Definitions() {
    static const std::map<std::u16string_view, Definition, std::less<>> definitions{
        {u"AbortSignalAnyOperationEnabled", {false, FeatureKind::Testable}},
        {u"AcceleratedCompositingEnabled", {true, FeatureKind::Boolean}},
        {u"AcceleratedCompositingForFixedPositionEnabled", {false, FeatureKind::Boolean}},
        {u"AcceleratedDrawingEnabled", {true, FeatureKind::Internal}},
        {u"AccentColorEnabled", {false, FeatureKind::Testable}},
        {u"AccessHandleEnabled", {false, FeatureKind::Testable}},
        {u"AggressiveTileRetentionEnabled", {false, FeatureKind::Boolean}},
        {u"AllowContentSecurityPolicySourceStarToMatchAnyProtocol", {false, FeatureKind::Boolean}},
        {u"AllowCrossOriginSubresourcesToAskForCredentials", {false, FeatureKind::Boolean}},
        {u"AllowDisplayOfInsecureContent", {false, FeatureKind::Boolean}},
        {u"AllowFileAccessFromFileURLs", {false, FeatureKind::Boolean}},
        {u"AllowMediaContentTypesRequiringHardwareSupportAsFallback", {false, FeatureKind::Boolean}},
        {u"AllowMultiElementImplicitSubmission", {false, FeatureKind::Boolean}},
        {u"AllowRunningOfInsecureContent", {false, FeatureKind::Boolean}},
        {u"AllowSettingAnyXHRHeaderFromFileURLs", {false, FeatureKind::Boolean}},
        {u"AllowTopNavigationToDataURLs", {false, FeatureKind::Boolean}},
        {u"AllowUniversalAccessFromFileURLs", {false, FeatureKind::Boolean}},
        {u"AllowsInlineMediaPlayback", {true, FeatureKind::Boolean}},
        {u"AllowsInlineMediaPlaybackAfterFullscreen", {false, FeatureKind::Boolean}},
        {u"AllowsPictureInPictureMediaPlayback", {false, FeatureKind::Boolean}},
        {u"AnimatedImageAsyncDecodingEnabled", {true, FeatureKind::Boolean}},
        {u"AppBadgeEnabled", {false, FeatureKind::Internal}},
        {u"AppleMailPaginationQuirkEnabled", {false, FeatureKind::Boolean}},
        {u"AriaReflectionForElementReferencesEnabled", {true, FeatureKind::Testable}},
        {u"AspectRatioEnabled", {true, FeatureKind::Testable}},
        {u"AspectRatioOfImgFromWidthAndHeightEnabled", {true, FeatureKind::Testable}},
        {u"AsyncClipboardAPIEnabled", {false, FeatureKind::Testable}},
        {u"AsyncFrameScrollingEnabled", {true, FeatureKind::Internal}},
        {u"AsyncOverflowScrollingEnabled", {true, FeatureKind::Internal}},
        {u"AsynchronousSpellCheckingEnabled", {false, FeatureKind::Boolean}},
        {u"AudioControlsScaleWithPageZoom", {true, FeatureKind::Boolean}},
        {u"AudioDescriptionsEnabled", {false, FeatureKind::Testable}},
        {u"AuthorAndUserStylesEnabled", {true, FeatureKind::Boolean}},
        {u"AutomaticLiveResizeEnabled", {false, FeatureKind::Internal}},
        {u"BackspaceKeyNavigationEnabled", {true, FeatureKind::Boolean}},
        {u"BeaconAPIEnabled", {true, FeatureKind::Testable}},
        {u"BlobRegistryTopOriginPartitioningEnabled", {false, FeatureKind::Internal}},
        {u"BroadcastChannelEnabled", {true, FeatureKind::Testable}},
        {u"BroadcastChannelOriginPartitioningEnabled", {true, FeatureKind::Testable}},
        {u"CSS3DTransformInteroperabilityEnabled", {true, FeatureKind::Testable}},
        {u"CSSCascadeLayersEnabled", {true, FeatureKind::Testable}},
        {u"CSSColor4", {true, FeatureKind::Testable}},
        {u"CSSColorContrastEnabled", {false, FeatureKind::Testable}},
        {u"CSSColorMixEnabled", {true, FeatureKind::Testable}},
        {u"CSSContainIntrinsicSizeEnabled", {true, FeatureKind::Testable}},
        {u"CSSContainerQueriesEnabled", {true, FeatureKind::Testable}},
        {u"CSSContainmentEnabled", {true, FeatureKind::Testable}},
        {u"CSSContentVisibilityEnabled", {false, FeatureKind::Testable}},
        {u"CSSCounterStyleAtRuleImageSymbolsEnabled", {false, FeatureKind::Testable}},
        {u"CSSCounterStyleAtRulesEnabled", {true, FeatureKind::Testable}},
        {u"CSSCustomPropertiesAndValuesEnabled", {true, FeatureKind::Testable}},
        {u"CSSFontFaceSizeAdjustEnabled", {true, FeatureKind::Testable}},
        {u"CSSGradientInterpolationColorSpacesEnabled", {true, FeatureKind::Testable}},
        {u"CSSGradientPremultipliedAlphaInterpolationEnabled", {true, FeatureKind::Testable}},
        {u"CSSIndividualTransformPropertiesEnabled", {true, FeatureKind::Testable}},
        {u"CSSInputSecurityEnabled", {false, FeatureKind::Testable}},
        {u"CSSMarginTrimEnabled", {true, FeatureKind::Testable}},
        {u"CSSMotionPathEnabled", {true, FeatureKind::Testable}},
        {u"CSSNestingEnabled", {true, FeatureKind::Testable}},
        {u"CSSOMViewScrollingAPIEnabled", {false, FeatureKind::Testable}},
        {u"CSSOMViewSmoothScrollingEnabled", {false, FeatureKind::Testable}},
        {u"CSSRelativeColorSyntaxEnabled", {true, FeatureKind::Testable}},
        {u"CSSRhythmicSizingEnabled", {false, FeatureKind::Testable}},
        {u"CSSScrollAnchoringEnabled", {false, FeatureKind::Testable}},
        {u"CSSScrollbarColorEnabled", {false, FeatureKind::Testable}},
        {u"CSSScrollbarGutterEnabled", {false, FeatureKind::Testable}},
        {u"CSSScrollbarWidthEnabled", {false, FeatureKind::Testable}},
        {u"CSSTextAlignLastEnabled", {true, FeatureKind::Testable}},
        {u"CSSTextBoxTrimEnabled", {false, FeatureKind::Testable}},
        {u"CSSTextGroupAlignEnabled", {false, FeatureKind::Testable}},
        {u"CSSTextJustifyEnabled", {false, FeatureKind::Testable}},
        {u"CSSTextSpacingEnabled", {false, FeatureKind::Testable}},
        {u"CSSTextUnderlinePositionLeftRightEnabled", {false, FeatureKind::Internal}},
        {u"CSSTextWrapNewValuesEnabled", {false, FeatureKind::Testable}},
        {u"CSSTypedOMColorEnabled", {false, FeatureKind::Testable}},
        {u"CSSTypedOMEnabled", {true, FeatureKind::Testable}},
        {u"CSSWhiteSpaceLonghandsEnabled", {false, FeatureKind::Testable}},
        {u"CSSWordBreakAutoEnabled", {false, FeatureKind::Testable}},
        {u"CacheAPIEnabled", {true, FeatureKind::Boolean}},
        {u"CanvasColorSpaceEnabled", {false, FeatureKind::Testable}},
        {u"CanvasFiltersEnabled", {false, FeatureKind::Internal}},
        {u"CanvasUsesAcceleratedDrawing", {true, FeatureKind::Boolean}},
        {u"CaretBrowsingEnabled", {false, FeatureKind::Boolean}},
        {u"ClearSiteDataHTTPHeaderEnabled", {true, FeatureKind::Testable}},
        {u"ClientBadgeEnabled", {false, FeatureKind::Internal}},
        {u"ColorFilterEnabled", {false, FeatureKind::Boolean}},
        {u"CompositingBordersVisible", {false, FeatureKind::Internal}},
        {u"CompositingRepaintCountersVisible", {false, FeatureKind::Internal}},
        {u"CompressionStreamEnabled", {true, FeatureKind::Testable}},
        {u"ConstantPropertiesEnabled", {true, FeatureKind::Boolean}},
        {u"ConstructableStylesheetsEnabled", {true, FeatureKind::Testable}},
        {u"ContactPickerAPIEnabled", {false, FeatureKind::Testable}},
        {u"ContentDispositionAttachmentSandboxEnabled", {false, FeatureKind::Boolean}},
        {u"CookieConsentAPIEnabled", {false, FeatureKind::Internal}},
        {u"CookieEnabled", {true, FeatureKind::Boolean}},
        {u"CookieStoreAPIEnabled", {false, FeatureKind::Testable}},
        {u"CoreMathMLEnabled", {false, FeatureKind::Internal}},
        {u"CrossOriginEmbedderPolicyEnabled", {true, FeatureKind::Testable}},
        {u"CrossOriginOpenerPolicyEnabled", {true, FeatureKind::Testable}},
        {u"CursiveFontFamily", {Preferences::StringValue{u"Comic Sans MS"}, FeatureKind::None}},
        {u"CustomPasteboardDataEnabled", {false, FeatureKind::Boolean}},
        {u"DNSPrefetchingEnabled", {false, FeatureKind::Boolean}},
        {u"DOMPasteAccessRequestsEnabled", {false, FeatureKind::Internal}},
        {u"DOMPasteAllowed", {false, FeatureKind::Boolean}},
        {u"DOMTestingAPIsEnabled", {false, FeatureKind::Internal}},
        {u"DOMTimersThrottlingEnabled", {true, FeatureKind::Boolean}},
        {u"DataTransferItemsEnabled", {false, FeatureKind::Boolean}},
        {u"DatabasesEnabled", {true, FeatureKind::Boolean}},
        {u"DeclarativeShadowDOMEnabled", {true, FeatureKind::Testable}},
        {u"DeclarativeShadowDOMInDOMParserEnabled", {false, FeatureKind::Testable}},
        {u"DefaultARIAForCustomElementsEnabled", {true, FeatureKind::Testable}},
        {u"DefaultFixedFontSize", {double{13}, FeatureKind::None}},
        {u"DefaultFontSize", {double{16}, FeatureKind::None}},
        {u"DefaultTextEncodingName", {Preferences::StringValue{u"ISO-8859-1"}, FeatureKind::None}},
        {u"DeprecateRSAESPKCSWebCryptoEnabled", {true, FeatureKind::Testable}},
        {u"DeprecationReportingEnabled", {false, FeatureKind::Testable}},
        {u"DeveloperExtrasEnabled", {false, FeatureKind::Boolean}},
        {u"DeviceHeight", {std::uint32_t{0}, FeatureKind::None}},
        {u"DeviceWidth", {std::uint32_t{0}, FeatureKind::None}},
        {u"DiagnosticLoggingEnabled", {false, FeatureKind::Boolean}},
        {u"DialogElementEnabled", {true, FeatureKind::Testable}},
        {u"DirPseudoEnabled", {true, FeatureKind::Testable}},
        {u"DirectoryUploadEnabled", {false, FeatureKind::Boolean}},
        {u"DisabledAdaptationsMetaTagEnabled", {false, FeatureKind::Boolean}},
        {u"DisallowSyncXHRDuringPageDismissalEnabled", {true, FeatureKind::Testable}},
        {u"DisplayListDrawingEnabled", {false, FeatureKind::Internal}},
        {u"EditableLinkBehavior", {std::uint32_t{4}, FeatureKind::None}},
        {u"EmbedElementEnabled", {true, FeatureKind::Testable}},
        {u"EnableInheritURIQueryComponent", {false, FeatureKind::Boolean}},
        {u"EncryptedMediaAPIEnabled", {std::monostate{}, FeatureKind::Unverified}},
        {u"EnterKeyHintEnabled", {false, FeatureKind::Internal}},
        {u"EnumeratingAllNetworkInterfacesEnabled", {false, FeatureKind::Internal}},
        {u"EnumeratingVisibleNetworkInterfacesEnabled", {false, FeatureKind::Internal}},
        {u"EventHandlerDrivenSmoothKeyboardScrollingEnabled", {false, FeatureKind::Internal}},
        {u"ExtendedAudioDescriptionsEnabled", {false, FeatureKind::Testable}},
        {u"FTPDirectoryTemplatePath", {Preferences::StringValue{u""}, FeatureKind::None}},
        {u"FTPEnabled", {false, FeatureKind::Boolean}},
        {u"FantasyFontFamily", {Preferences::StringValue{u"Impact"}, FeatureKind::None}},
        {u"FetchAPIEnabled", {true, FeatureKind::Testable}},
        {u"FetchAPIKeepAliveEnabled", {true, FeatureKind::Testable}},
        {u"FetchMetadataEnabled", {true, FeatureKind::Testable}},
        {u"FetchPriorityEnabled", {false, FeatureKind::Testable}},
        {u"FileReaderAPIEnabled", {true, FeatureKind::Boolean}},
        {u"FileSystemAccessEnabled", {false, FeatureKind::Testable}},
        {u"FixedFontFamily", {Preferences::StringValue{u"Courier New"}, FeatureKind::None}},
        {u"FlexFormattingContextIntegrationEnabled", {false, FeatureKind::Internal}},
        {u"FocusVisibleEnabled", {true, FeatureKind::Testable}},
        {u"ForceCompositingMode", {false, FeatureKind::Boolean}},
        {u"ForceFTPDirectoryListings", {false, FeatureKind::Boolean}},
        {u"ForceWebGLUsesLowPower", {false, FeatureKind::Boolean}},
        {u"FormAssociatedCustomElementsEnabled", {true, FeatureKind::Testable}},
        {u"FrameFlatteningEnabled", {false, FeatureKind::Boolean}},
        {u"FullScreenEnabled", {false, FeatureKind::Boolean}},
        {u"FullscreenRequirementForScreenOrientationLockingEnabled", {true, FeatureKind::Boolean}},
        {u"GenericCueAPIEnabled", {true, FeatureKind::Testable}},
        {u"GoogleAntiFlickerOptimizationQuirkEnabled", {true, FeatureKind::Testable}},
        {u"HTTPEquivEnabled", {true, FeatureKind::Testable}},
        {u"HasPseudoClassEnabled", {true, FeatureKind::Testable}},
        {u"HiddenPageCSSAnimationSuspensionEnabled", {false, FeatureKind::Boolean}},
        {u"HiddenPageDOMTimerThrottlingAutoIncreases", {false, FeatureKind::Boolean}},
        {u"HiddenPageDOMTimerThrottlingEnabled", {false, FeatureKind::Boolean}},
        {u"HighlightAPIEnabled", {false, FeatureKind::Testable}},
        {u"HyperlinkAuditingEnabled", {true, FeatureKind::Boolean}},
        {u"ICECandidateFilteringEnabled", {true, FeatureKind::Internal}},
        {u"ImperativeSlotAPIEnabled", {true, FeatureKind::Testable}},
        {u"InactiveMediaCaptureSteamRepromptIntervalInMinutes", {double{10}, FeatureKind::None}},
        {u"IncompleteImageBorderEnabled", {false, FeatureKind::Boolean}},
        {u"IncrementalRenderingSuppressionTimeout", {double{5}, FeatureKind::None}},
        {u"IndexedDBAPIEnabled", {true, FeatureKind::Boolean}},
        {u"IndexedDBAPIExposeIndexedDBToWindowOrWorkerEnabled", {false, FeatureKind::Internal}},
        {u"InertAttributeEnabled", {true, FeatureKind::Testable}},
        {u"InlineFormattingContextIntegrationEnabled", {true, FeatureKind::Testable}},
        {u"InlineMediaPlaybackRequiresPlaysInlineAttribute", {false, FeatureKind::Boolean}},
        {u"InlinePredictionsEnabled", {false, FeatureKind::Boolean}},
        {u"InspectorAttachedHeight", {std::uint32_t{500}, FeatureKind::None}},
        {u"InspectorAttachedWidth", {std::uint32_t{750}, FeatureKind::None}},
        {u"InspectorAttachmentSide", {std::uint32_t{0}, FeatureKind::None}},
        {u"InspectorStartsAttached", {true, FeatureKind::Boolean}},
        {u"InspectorWindowFrame", {Preferences::StringValue{u""}, FeatureKind::None}},
        {u"InteractionRegionInlinePadding", {double{4}, FeatureKind::None}},
        {u"InteractionRegionMinimumCornerRadius", {double{8}, FeatureKind::None}},
        {u"InteractiveFormValidationEnabled", {true, FeatureKind::Boolean}},
        {u"IntersectionObserverEnabled", {true, FeatureKind::Testable}},
        {u"InvisibleAutoplayNotPermitted", {false, FeatureKind::Boolean}},
        {u"IsFirstPartyWebsiteDataRemovalDisabled", {false, FeatureKind::Boolean}},
        {u"IsFirstPartyWebsiteDataRemovalLiveOnTestingEnabled", {false, FeatureKind::Boolean}},
        {u"IsFirstPartyWebsiteDataRemovalReproTestingEnabled", {false, FeatureKind::Boolean}},
        {u"IsLoggedInAPIEnabled", {false, FeatureKind::Testable}},
        {u"IsSameSiteStrictEnforcementEnabled", {false, FeatureKind::Internal}},
        {u"IsThirdPartyCookieBlockingDisabled", {false, FeatureKind::Boolean}},
        {u"ItpDebugModeEnabled", {false, FeatureKind::Boolean}},
        {u"JavaScriptCanAccessClipboard", {false, FeatureKind::Boolean}},
        {u"JavaScriptCanOpenWindowsAutomatically", {true, FeatureKind::Boolean}},
        {u"JavaScriptEnabled", {true, FeatureKind::Testable}},
        {u"JavaScriptMarkupEnabled", {true, FeatureKind::Boolean}},
        {u"JavaScriptRuntimeFlags", {std::uint32_t{0}, FeatureKind::None}},
        {u"LargeImageAsyncDecodingEnabled", {true, FeatureKind::Boolean}},
        {u"LayoutFallbackWidth", {std::uint32_t{980}, FeatureKind::None}},
        {u"LayoutFormattingContextEnabled", {false, FeatureKind::Internal}},
        {u"LayoutViewportHeightExpansionFactor", {double{0}, FeatureKind::None}},
        {u"LazyIframeLoadingEnabled", {true, FeatureKind::Testable}},
        {u"LazyImageLoadingEnabled", {true, FeatureKind::Testable}},
        {u"LegacyLineLayoutVisualCoverageEnabled", {false, FeatureKind::Internal}},
        {u"LineHeightUnitsEnabled", {true, FeatureKind::Testable}},
        {u"LinkModulePreloadEnabled", {true, FeatureKind::Testable}},
        {u"LinkPreconnect", {false, FeatureKind::Boolean}},
        {u"LinkPreconnectEarlyHintsEnabled", {false, FeatureKind::Testable}},
        {u"LinkPrefetchEnabled", {false, FeatureKind::Testable}},
        {u"LinkPreloadEnabled", {true, FeatureKind::Boolean}},
        {u"LinkPreloadResponsiveImagesEnabled", {false, FeatureKind::Testable}},
        {u"LinkSanitizerEnabled", {false, FeatureKind::Internal}},
        {u"LiveRangeSelectionEnabled", {true, FeatureKind::Testable}},
        {u"LoadDeferringEnabled", {true, FeatureKind::Boolean}},
        {u"LoadsImagesAutomatically", {true, FeatureKind::Boolean}},
        {u"LocalFileContentSniffingEnabled", {false, FeatureKind::Internal}},
        {u"LocalStorageEnabled", {true, FeatureKind::Testable}},
        {u"LogsPageMessagesToSystemConsoleEnabled", {false, FeatureKind::Internal}},
        {u"LongRunningMediaCaptureStreamRepromptIntervalInHours", {double{24}, FeatureKind::None}},
        {u"LowPowerVideoAudioBufferSizeEnabled", {true, FeatureKind::Boolean}},
        {u"MainContentUserGestureOverrideEnabled", {false, FeatureKind::Boolean}},
        {u"MasonryEnabled", {false, FeatureKind::Testable}},
        {u"MasonryTrackAlignmentEnabled", {false, FeatureKind::Testable}},
        {u"MaxParseDuration", {double{-1}, FeatureKind::None}},
        {u"MediaAudioCodecIDsAllowedInLockdownMode", {Preferences::StringValue{u"aac ,zaac,qaac,caac,.mp3,mp4a"}, FeatureKind::None}},
        {u"MediaCapabilitiesEnabled", {true, FeatureKind::Boolean}},
        {u"MediaCapabilitiesExtensionsEnabled", {true, FeatureKind::Testable}},
        {u"MediaCaptionFormatTypesAllowedInLockdownMode", {Preferences::StringValue{u"c608,wvtt"}, FeatureKind::None}},
        {u"MediaCodecTypesAllowedInLockdownMode", {Preferences::StringValue{u"mp4a.40,avc1"}, FeatureKind::None}},
        {u"MediaContainerTypesAllowedInLockdownMode", {Preferences::StringValue{u"video/mp4,audio/mp4,video/x-m4v,audio/x-m4a,audio/mp3,application/x-mpegURL,application/vnd.apple.mpegURL,video/mp2t,video/iso.segment,audio/aac,audio/mpeg,audio/ac3,audio/eac3,video/mpeg2,text/vtt"}, FeatureKind::None}},
        {u"MediaContentTypesRequiringHardwareSupport", {Preferences::StringValue{u""}, FeatureKind::None}},
        {u"MediaControlsScaleWithPageZoom", {true, FeatureKind::Boolean}},
        {u"MediaDataLoadsAutomatically", {true, FeatureKind::Boolean}},
        {u"MediaEnabled", {true, FeatureKind::Boolean}},
        {u"MediaPlayable", {true, FeatureKind::Boolean}},
        {u"MediaPreferredFullscreenWidth", {double{960}, FeatureKind::None}},
        {u"MediaPreloadingEnabled", {false, FeatureKind::Boolean}},
        {u"MediaSourceEnabled", {std::monostate{}, FeatureKind::Boolean}},
        {u"MediaUserGestureInheritsFromDocument", {false, FeatureKind::Boolean}},
        {u"MediaVideoCodecIDsAllowedInLockdownMode", {Preferences::StringValue{u"avc1,zavc,qavc,cavc"}, FeatureKind::None}},
        {u"MenuItemElementEnabled", {false, FeatureKind::Boolean}},
        {u"MinimumFontSize", {double{0}, FeatureKind::None}},
        {u"MinimumLogicalFontSize", {double{9}, FeatureKind::None}},
        {u"MockCaptureDevicesPromptEnabled", {true, FeatureKind::Boolean}},
        {u"MockScrollbarsControllerEnabled", {false, FeatureKind::Boolean}},
        {u"MockScrollbarsEnabled", {false, FeatureKind::Boolean}},
        {u"MomentumScrollingAnimatorEnabled", {true, FeatureKind::Internal}},
        {u"NeedsAdobeFrameReloadingQuirk", {false, FeatureKind::Boolean}},
        {u"NeedsFrameNameFallbackToIdQuirk", {false, FeatureKind::Boolean}},
        {u"NeedsKeyboardEventDisambiguationQuirks", {false, FeatureKind::Boolean}},
        {u"NeedsSiteSpecificQuirks", {true, FeatureKind::Boolean}},
        {u"NeedsStorageAccessFromFileURLsQuirk", {true, FeatureKind::Boolean}},
        {u"OfflineWebApplicationCacheEnabled", {true, FeatureKind::Boolean}},
        {u"OverflowClipEnabled", {true, FeatureKind::Testable}},
        {u"OverscrollBehaviorEnabled", {true, FeatureKind::Testable}},
        {u"PageAtRuleSupportEnabled", {false, FeatureKind::Internal}},
        {u"PageVisibilityBasedProcessSuppressionEnabled", {true, FeatureKind::Internal}},
        {u"PaintTimingEnabled", {true, FeatureKind::Testable}},
        {u"PassiveTouchListenersAsDefaultOnDocument", {true, FeatureKind::Boolean}},
        {u"PassiveWheelListenersAsDefaultOnDocument", {true, FeatureKind::Boolean}},
        {u"PasswordEchoDuration", {double{2}, FeatureKind::None}},
        {u"PasswordEchoEnabled", {false, FeatureKind::Boolean}},
        {u"PerformanceNavigationTimingAPIEnabled", {true, FeatureKind::Testable}},
        {u"PerformanceResourceTimingSensitivePropertiesEnabled", {true, FeatureKind::Testable}},
        {u"PermissionsAPIEnabled", {false, FeatureKind::Testable}},
        {u"PictographFontFamily", {Preferences::StringValue{u"Times"}, FeatureKind::None}},
        {u"PitchCorrectionAlgorithm", {std::uint32_t{0}, FeatureKind::None}},
        {u"PlayStationMediaPlayerEnabled", {true, FeatureKind::Boolean}},
        {u"PluginsEnabled", {false, FeatureKind::Boolean}},
        {u"PopoverAttributeEnabled", {false, FeatureKind::Testable}},
        {u"PreferPageRenderingUpdatesNear60FPSEnabled", {true, FeatureKind::Boolean}},
        {u"PreferSandboxedMediaParsing", {true, FeatureKind::Internal}},
        {u"PrivateClickMeasurementDebugModeEnabled", {false, FeatureKind::Boolean}},
        {u"PrivateClickMeasurementEnabled", {true, FeatureKind::Boolean}},
        {u"PrivateClickMeasurementFraudPreventionEnabled", {false, FeatureKind::Testable}},
        {u"ProcessSwapOnCrossSiteNavigationEnabled", {false, FeatureKind::Testable}},
        {u"ProcessSwapOnCrossSiteWindowOpenEnabled", {false, FeatureKind::Internal}},
        {u"PunchOutWhiteBackgroundsInDarkMode", {false, FeatureKind::Boolean}},
        {u"ReadableByteStreamAPIEnabled", {false, FeatureKind::Testable}},
        {u"ReferrerPolicyAttributeEnabled", {true, FeatureKind::Testable}},
        {u"ReportingEnabled", {false, FeatureKind::Testable}},
        {u"RequestIdleCallbackEnabled", {false, FeatureKind::Internal}},
        {u"RequestSubmitEnabled", {true, FeatureKind::Testable}},
        {u"RequestVideoFrameCallbackEnabled", {false, FeatureKind::Testable}},
        {u"RequiresPageVisibilityToPlayAudio", {false, FeatureKind::Boolean}},
        {u"RequiresUserGestureForAudioPlayback", {false, FeatureKind::Boolean}},
        {u"RequiresUserGestureForMediaPlayback", {false, FeatureKind::Boolean}},
        {u"RequiresUserGestureForVideoPlayback", {false, FeatureKind::Boolean}},
        {u"RequiresUserGestureToLoadVideo", {false, FeatureKind::Boolean}},
        {u"ResizeObserverEnabled", {true, FeatureKind::Testable}},
        {u"ResourceLoadSchedulingEnabled", {true, FeatureKind::Internal}},
        {u"ResourceUsageOverlayVisible", {false, FeatureKind::Internal}},
        {u"RestrictedHTTPResponseAccess", {true, FeatureKind::Internal}},
        {u"SKAttributionEnabled", {true, FeatureKind::Boolean}},
        {u"SafeBrowsingEnabled", {true, FeatureKind::Boolean}},
        {u"SampledPageTopColorMaxDifference", {double{0}, FeatureKind::None}},
        {u"SampledPageTopColorMinHeight", {double{0}, FeatureKind::None}},
        {u"SansSerifFontFamily", {Preferences::StringValue{u"Helvetica"}, FeatureKind::None}},
        {u"ScreenOrientationAPIEnabled", {false, FeatureKind::Testable}},
        {u"ScreenOrientationLockingAPIEnabled", {false, FeatureKind::Testable}},
        {u"ScreenWakeLockAPIEnabled", {true, FeatureKind::Testable}},
        {u"ScrollAnimatorEnabled", {false, FeatureKind::Boolean}},
        {u"ScrollToTextFragmentEnabled", {true, FeatureKind::Testable}},
        {u"ScrollToTextFragmentIndicatorEnabled", {true, FeatureKind::Boolean}},
        {u"ScrollingPerformanceTestingEnabled", {false, FeatureKind::Internal}},
        {u"SecureContextChecksEnabled", {true, FeatureKind::Internal}},
        {u"SelectTrailingWhitespaceEnabled", {false, FeatureKind::Boolean}},
        {u"SelectionAPIForShadowDOMEnabled", {true, FeatureKind::Testable}},
        {u"SelectionFlippingEnabled", {true, FeatureKind::Boolean}},
        {u"SendMouseEventsToDisabledFormControlsEnabled", {false, FeatureKind::Testable}},
        {u"SerifFontFamily", {Preferences::StringValue{u"Times"}, FeatureKind::None}},
        {u"ServerTimingEnabled", {true, FeatureKind::Testable}},
        {u"ShapeDetection", {false, FeatureKind::Testable}},
        {u"SharedWorkerEnabled", {true, FeatureKind::Testable}},
        {u"ShouldAllowUserInstalledFonts", {true, FeatureKind::Boolean}},
        {u"ShouldConvertInvalidURLsToBlank", {true, FeatureKind::Boolean}},
        {u"ShouldConvertPositionStyleOnCopy", {false, FeatureKind::Boolean}},
        {u"ShouldDecidePolicyBeforeLoadingQuickLookPreview", {false, FeatureKind::Boolean}},
        {u"ShouldDeferAsynchronousScriptsUntilAfterDocumentLoadOrFirstPaint", {true, FeatureKind::Testable}},
        {u"ShouldDisplayCaptions", {false, FeatureKind::Boolean}},
        {u"ShouldDisplaySubtitles", {false, FeatureKind::Boolean}},
        {u"ShouldDisplayTextDescriptions", {false, FeatureKind::Boolean}},
        {u"ShouldDropNearSuspendedAssertionAfterDelay", {false, FeatureKind::Internal}},
        {u"ShouldIgnoreMetaViewport", {false, FeatureKind::Boolean}},
        {u"ShouldPrintBackgrounds", {false, FeatureKind::Boolean}},
        {u"ShouldRespectImageOrientation", {false, FeatureKind::Boolean}},
        {u"ShouldRestrictBaseURLSchemes", {false, FeatureKind::Boolean}},
        {u"ShouldSuppressTextInputFromEditingDuringProvisionalNavigation", {false, FeatureKind::Boolean}},
        {u"ShouldTakeNearSuspendedAssertions", {true, FeatureKind::Internal}},
        {u"ShouldUseServiceWorkerShortTimeout", {false, FeatureKind::Boolean}},
        {u"ShowMediaStatsContextMenuItemEnabled", {false, FeatureKind::Boolean}},
        {u"ShowModalDialogEnabled", {false, FeatureKind::Boolean}},
        {u"ShowsToolTipOverTruncatedText", {false, FeatureKind::Boolean}},
        {u"ShowsURLsInToolTipsEnabled", {false, FeatureKind::Boolean}},
        {u"ShrinksStandaloneImagesToFit", {true, FeatureKind::Boolean}},
        {u"SiteIsolationEnabled", {false, FeatureKind::Internal}},
        {u"SmartInsertDeleteEnabled", {true, FeatureKind::Boolean}},
        {u"SourceBufferChangeTypeEnabled", {std::monostate{}, FeatureKind::Unverified}},
        {u"SpatialNavigationEnabled", {false, FeatureKind::Boolean}},
        {u"SpeechRecognitionEnabled", {false, FeatureKind::Testable}},
        {u"SpeechSynthesisAPIEnabled", {true, FeatureKind::Boolean}},
        {u"SpringTimingFunctionEnabled", {false, FeatureKind::Internal}},
        {u"Standalone", {false, FeatureKind::Boolean}},
        {u"StandardFontFamily", {Preferences::StringValue{u"Times"}, FeatureKind::None}},
        {u"StorageAPIEnabled", {false, FeatureKind::Testable}},
        {u"StorageAPIEstimateEnabled", {false, FeatureKind::Testable}},
        {u"StorageAccessAPIEnabled", {true, FeatureKind::Testable}},
        {u"StorageBlockingPolicy", {std::uint32_t{1}, FeatureKind::None}},
        {u"StrictSecureDecodingForAllObjC", {true, FeatureKind::Internal}},
        {u"SubgridEnabled", {true, FeatureKind::Testable}},
        {u"SuppressesIncrementalRendering", {false, FeatureKind::Boolean}},
        {u"SyntheticEditingCommandsEnabled", {true, FeatureKind::Testable}},
        {u"SystemLayoutDirection", {std::uint32_t{0}, FeatureKind::None}},
        {u"TabsToLinks", {false, FeatureKind::Boolean}},
        {u"TelephoneNumberParsingEnabled", {false, FeatureKind::Boolean}},
        {u"TemporaryTileCohortRetentionEnabled", {true, FeatureKind::Boolean}},
        {u"TextAreasAreResizable", {true, FeatureKind::Boolean}},
        {u"TextInteractionEnabled", {true, FeatureKind::Boolean}},
        {u"ThirdPartyIframeRedirectBlockingEnabled", {true, FeatureKind::Testable}},
        {u"ThreadedScrollingEnabled", {true, FeatureKind::Boolean}},
        {u"TiledScrollingIndicatorVisible", {false, FeatureKind::Internal}},
        {u"TrackConfigurationEnabled", {false, FeatureKind::Testable}},
        {u"TransformStreamAPIEnabled", {true, FeatureKind::Testable}},
        {u"TreatsAnyTextCSSLinkAsStylesheet", {false, FeatureKind::Boolean}},
        {u"UndoManagerAPIEnabled", {false, FeatureKind::Internal}},
        {u"UnprefixedFullscreenAPIEnabled", {true, FeatureKind::Testable}},
        {u"UpgradeKnownHostsToHTTPSEnabled", {true, FeatureKind::Internal}},
        {u"UseGeneralDirectoryForStorage", {true, FeatureKind::Internal}},
        {u"UseGiantTiles", {false, FeatureKind::Boolean}},
        {u"UseImageDocumentForSubframePDF", {false, FeatureKind::Boolean}},
        {u"UseLegacyBackgroundSizeShorthandBehavior", {false, FeatureKind::Boolean}},
        {u"UsePreHTML5ParserQuirks", {false, FeatureKind::Boolean}},
        {u"UserActivationAPIEnabled", {true, FeatureKind::Testable}},
        {u"UserGesturePromisePropagationEnabled", {true, FeatureKind::Internal}},
        {u"UserInterfaceDirectionPolicy", {std::uint32_t{0}, FeatureKind::None}},
        {u"UsesBackForwardCache", {true, FeatureKind::Boolean}},
        {u"UsesEncodingDetector", {false, FeatureKind::Boolean}},
        {u"VerifyWindowOpenUserGestureFromUIProcess", {false, FeatureKind::Testable}},
        {u"VerticalFormControlsEnabled", {false, FeatureKind::Testable}},
        {u"VideoPresentationModeAPIEnabled", {true, FeatureKind::Boolean}},
        {u"VideoQualityIncludesDisplayCompositingEnabled", {false, FeatureKind::Boolean}},
        {u"ViewGestureDebuggingEnabled", {false, FeatureKind::Boolean}},
        {u"VisibleDebugOverlayRegions", {std::uint32_t{0}, FeatureKind::None}},
        {u"VisualViewportAPIEnabled", {true, FeatureKind::Testable}},
        {u"VisualViewportEnabled", {true, FeatureKind::Boolean}},
        {u"WantsBalancedSetDefersLoadingBehavior", {false, FeatureKind::Boolean}},
        {u"WebAPIStatisticsEnabled", {false, FeatureKind::Internal}},
        {u"WebAPIsInShadowRealmEnabled", {false, FeatureKind::Internal}},
        {u"WebAnimationsCompositeOperationsEnabled", {true, FeatureKind::Testable}},
        {u"WebAnimationsCustomEffectsEnabled", {false, FeatureKind::Testable}},
        {u"WebAnimationsCustomFrameRateEnabled", {false, FeatureKind::Testable}},
        {u"WebAnimationsIterationCompositeEnabled", {true, FeatureKind::Testable}},
        {u"WebAnimationsMutableTimelinesEnabled", {true, FeatureKind::Testable}},
        {u"WebCryptoSafeCurvesEnabled", {false, FeatureKind::Testable}},
        {u"WebGLDraftExtensionsEnabled", {false, FeatureKind::Internal}},
        {u"WebGLEnabled", {true, FeatureKind::Testable}},
        {u"WebGLTimerQueriesEnabled", {false, FeatureKind::Boolean}},
        {u"WebGPUEnabled", {false, FeatureKind::Internal}},
        {u"WebInspectorEngineeringSettingsAllowed", {false, FeatureKind::Internal}},
        {u"WebLocksAPIEnabled", {true, FeatureKind::Testable}},
        {u"WebMParserEnabled", {std::monostate{}, FeatureKind::Unverified}},
        {u"WebMusicPlayerEnabled", {false, FeatureKind::Boolean}},
        {u"WebRTCAudioLatencyAdaptationEnabled", {true, FeatureKind::Internal}},
        {u"WebSecurityEnabled", {true, FeatureKind::Boolean}},
        {u"WebShareEnabled", {false, FeatureKind::Boolean}},
        {u"WebShareFileAPIEnabled", {false, FeatureKind::Testable}},
        {u"WebSocketEnabled", {true, FeatureKind::Boolean}},
        {u"WheelEventGesturesBecomeNonBlocking", {true, FeatureKind::Testable}},
        {u"WindowFocusRestricted", {true, FeatureKind::Boolean}},
        {u"WirelessPlaybackTargetAPIEnabled", {true, FeatureKind::Boolean}},
    };
    return definitions;
}

}

Preferences::Preferences(StringValue identifier) : identifier(std::move(identifier)) {
    values.emplace(u"ShouldRestrictBaseURLSchemes", true);
    overriddenDefaults.emplace(u"PluginsEnabled", true);
    overriddenDefaults.emplace(u"StorageBlockingPolicy", std::uint32_t{0});
}

Preferences::Preferences(const Preferences& other) : values(other.values), overriddenDefaults(other.overriddenDefaults) {
    values.insert_or_assign(u"ShouldRestrictBaseURLSchemes", true);
}

const Preferences::Value* Preferences::findValue(std::u16string_view key, std::size_t type) const {
    if (const auto it = values.find(key); it != values.end() && it->second.index() == type) return &it->second;
    if (const auto it = overriddenDefaults.find(key); it != overriddenDefaults.end() && it->second.index() == type) return &it->second;
    const auto it = Definitions().find(key);
    if (it == Definitions().end()) return nullptr;
    if (std::holds_alternative<std::monostate>(it->second.value))
        throw std::runtime_error("WKPreferences: proprietary PlayStation media-player build configuration is unknown");
    return &it->second.value;
}

bool Preferences::GetBool(std::u16string_view key) const {
    const auto* value = findValue(key, 1);
    if (value) {
        if (const auto* result = std::get_if<bool>(value)) return *result;
    }
    return false;
}

std::uint32_t Preferences::GetUInt32(std::u16string_view key) const {
    const auto* value = findValue(key, 2);
    if (value) {
        if (const auto* result = std::get_if<std::uint32_t>(value)) return *result;
    }
    return 0;
}

double Preferences::GetDouble(std::u16string_view key) const {
    const auto* value = findValue(key, 3);
    if (value) {
        if (const auto* result = std::get_if<double>(value)) return *result;
    }
    return 0;
}

Preferences::StringValue Preferences::GetString(std::u16string_view key) const {
    const auto* value = findValue(key, 4);
    if (value) {
        if (const auto* result = std::get_if<StringValue>(value)) return *result;
    }
    return std::nullopt;
}

void Preferences::Set(std::u16string_view key, Value value) {
    const Value* existing = nullptr;
    if (const auto it = values.find(key); it != values.end() && it->second.index() == value.index())
        existing = &it->second;
    if (!existing) {
        if (const auto it = overriddenDefaults.find(key); it != overriddenDefaults.end() && it->second.index() == value.index())
            existing = &it->second;
    }
    if (!existing) {
        if (const auto it = Definitions().find(key); it != Definitions().end()) {
            if (it->second.value.index() == value.index()) existing = &it->second.value;
            else if (std::holds_alternative<std::monostate>(it->second.value)) {
                values.insert_or_assign(std::u16string(key), std::move(value));
                return;
            }
        }
    }
    if (existing && *existing == value) return;
    if (!existing && (value == Value(false) || value == Value(std::uint32_t{0})
        || value == Value(double{0}) || value == Value(StringValue{}))) return;
    values.insert_or_assign(std::u16string(key), std::move(value));
}

void Preferences::SetFeature(std::u16string_view key, bool value) {
    const auto it = Definitions().find(key);
    if (it == Definitions().end() || it->second.feature == FeatureKind::None) return;
    if (it->second.feature == FeatureKind::Unverified)
        throw std::runtime_error("WKPreferences: media feature availability is unknown");
    Set(key, value);
}

void Preferences::EnableAllExperimentalFeatures() {
    for (const auto& [key, definition] : Definitions()) {
        if (definition.feature == FeatureKind::Testable) Set(key, true);
    }
}

void Preferences::ResetAllInternalDebugFeatures() {
    for (const auto& [key, definition] : Definitions()) {
        if (definition.feature == FeatureKind::Internal) Set(key, definition.value);
    }
}

}  // namespace NKWebKit

namespace {

NKWebKit::Preferences* PreferencesFromRef(const void* ref, const char* function) {
    return NKWebKit::Cast<NKWebKit::Preferences>(ref, function);
}

NKWebKit::Preferences::StringValue StringFromRef(const void* ref, const char* function) {
    const auto* string = NKWebKit::CastOrNull<NKWebKit::String>(ref, function);
    if (!string || string->null) return std::nullopt;
    return string->characters;
}

std::uint32_t FontSize(double value) {
    if (!std::isfinite(value) || value <= -1 || value >= double{std::numeric_limits<std::uint32_t>::max()} + 1)
        throw std::out_of_range("WKPreferences: font size cannot be represented by uint32_t");
    return static_cast<std::uint32_t>(value);
}

}

extern "C" {
const void* APS5_VABI WKPreferencesCreate() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Preferences>());
}

const void* APS5_VABI WKPreferencesCreateWithIdentifier(const void* identifierRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Preferences>(StringFromRef(identifierRef, __func__)));
}

const void* APS5_VABI WKPreferencesCreateCopy(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Preferences>(*PreferencesFromRef(preferencesRef, __func__)));
}

void APS5_VABI WKPreferencesEnableAllExperimentalFeatures(const void* preferencesRef) {
    PreferencesFromRef(preferencesRef, __func__)->EnableAllExperimentalFeatures();
}

void APS5_VABI WKPreferencesSetExperimentalFeatureForKey(const void* preferencesRef, bool value, const void* experimentalFeatureKey) {
    auto* preferences = PreferencesFromRef(preferencesRef, __func__);
    const auto key = StringFromRef(experimentalFeatureKey, __func__);
    if (key) preferences->SetFeature(*key, value);
}

void APS5_VABI WKPreferencesResetAllInternalDebugFeatures(const void* preferencesRef) {
    PreferencesFromRef(preferencesRef, __func__)->ResetAllInternalDebugFeatures();
}

void APS5_VABI WKPreferencesSetInternalDebugFeatureForKey(const void* preferencesRef, bool value, const void* internalDebugFeatureKey) {
    auto* preferences = PreferencesFromRef(preferencesRef, __func__);
    const auto key = StringFromRef(internalDebugFeatureKey, __func__);
    if (key) preferences->SetFeature(*key, value);
}

void APS5_VABI WKPreferencesResetTestRunnerOverrides(const void* preferencesRef) {
    PreferencesFromRef(preferencesRef, __func__);
}

void APS5_VABI WKPreferencesSetJavaScriptEnabled(const void* preferencesRef, bool javaScriptEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"JavaScriptEnabled", javaScriptEnabled);
}

bool APS5_VABI WKPreferencesGetJavaScriptEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"JavaScriptEnabled");
}

void APS5_VABI WKPreferencesSetJavaScriptMarkupEnabled(const void* preferencesRef, bool javaScriptMarkupEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"JavaScriptMarkupEnabled", javaScriptMarkupEnabled);
}

bool APS5_VABI WKPreferencesGetJavaScriptMarkupEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"JavaScriptMarkupEnabled");
}

void APS5_VABI WKPreferencesSetLoadsImagesAutomatically(const void* preferencesRef, bool loadsImagesAutomatically) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"LoadsImagesAutomatically", loadsImagesAutomatically);
}

bool APS5_VABI WKPreferencesGetLoadsImagesAutomatically(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"LoadsImagesAutomatically");
}

void APS5_VABI WKPreferencesSetOfflineWebApplicationCacheEnabled(const void* preferencesRef, bool offlineWebApplicationCacheEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"OfflineWebApplicationCacheEnabled", offlineWebApplicationCacheEnabled);
}

bool APS5_VABI WKPreferencesGetOfflineWebApplicationCacheEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"OfflineWebApplicationCacheEnabled");
}

void APS5_VABI WKPreferencesSetLocalStorageEnabled(const void* preferencesRef, bool localStorageEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"LocalStorageEnabled", localStorageEnabled);
}

bool APS5_VABI WKPreferencesGetLocalStorageEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"LocalStorageEnabled");
}

void APS5_VABI WKPreferencesSetDatabasesEnabled(const void* preferencesRef, bool databasesEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DatabasesEnabled", databasesEnabled);
}

bool APS5_VABI WKPreferencesGetDatabasesEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"DatabasesEnabled");
}

void APS5_VABI WKPreferencesSetFrameFlatteningEnabled(const void* preferencesRef, bool frameFlatteningEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"FrameFlatteningEnabled", frameFlatteningEnabled);
}

bool APS5_VABI WKPreferencesGetFrameFlatteningEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"FrameFlatteningEnabled");
}

void APS5_VABI WKPreferencesSetPluginsEnabled(const void* preferencesRef, bool pluginsEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"PluginsEnabled", pluginsEnabled);
}

bool APS5_VABI WKPreferencesGetPluginsEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"PluginsEnabled");
}

void APS5_VABI WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(const void* preferencesRef, bool javaScriptCanOpenWindowsAutomatically) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"JavaScriptCanOpenWindowsAutomatically", javaScriptCanOpenWindowsAutomatically);
}

bool APS5_VABI WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"JavaScriptCanOpenWindowsAutomatically");
}

void APS5_VABI WKPreferencesSetHyperlinkAuditingEnabled(const void* preferencesRef, bool hyperlinkAuditingEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"HyperlinkAuditingEnabled", hyperlinkAuditingEnabled);
}

bool APS5_VABI WKPreferencesGetHyperlinkAuditingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"HyperlinkAuditingEnabled");
}

void APS5_VABI WKPreferencesSetStandardFontFamily(const void* preferencesRef, const void* family) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"StandardFontFamily", StringFromRef(family, __func__));
}

const void* APS5_VABI WKPreferencesCopyStandardFontFamily(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"StandardFontFamily").value_or(std::u16string())));
}

void APS5_VABI WKPreferencesSetFixedFontFamily(const void* preferencesRef, const void* family) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"FixedFontFamily", StringFromRef(family, __func__));
}

const void* APS5_VABI WKPreferencesCopyFixedFontFamily(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"FixedFontFamily").value_or(std::u16string())));
}

void APS5_VABI WKPreferencesSetSerifFontFamily(const void* preferencesRef, const void* family) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"SerifFontFamily", StringFromRef(family, __func__));
}

const void* APS5_VABI WKPreferencesCopySerifFontFamily(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"SerifFontFamily").value_or(std::u16string())));
}

void APS5_VABI WKPreferencesSetSansSerifFontFamily(const void* preferencesRef, const void* family) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"SansSerifFontFamily", StringFromRef(family, __func__));
}

const void* APS5_VABI WKPreferencesCopySansSerifFontFamily(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"SansSerifFontFamily").value_or(std::u16string())));
}

void APS5_VABI WKPreferencesSetCursiveFontFamily(const void* preferencesRef, const void* family) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CursiveFontFamily", StringFromRef(family, __func__));
}

const void* APS5_VABI WKPreferencesCopyCursiveFontFamily(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"CursiveFontFamily").value_or(std::u16string())));
}

void APS5_VABI WKPreferencesSetFantasyFontFamily(const void* preferencesRef, const void* family) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"FantasyFontFamily", StringFromRef(family, __func__));
}

const void* APS5_VABI WKPreferencesCopyFantasyFontFamily(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"FantasyFontFamily").value_or(std::u16string())));
}

void APS5_VABI WKPreferencesSetPictographFontFamily(const void* preferencesRef, const void* family) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"PictographFontFamily", StringFromRef(family, __func__));
}

const void* APS5_VABI WKPreferencesCopyPictographFontFamily(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"PictographFontFamily").value_or(std::u16string())));
}

void APS5_VABI WKPreferencesSetDefaultFontSize(const void* preferencesRef, std::uint32_t size) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DefaultFontSize", static_cast<double>(size));
}

std::uint32_t APS5_VABI WKPreferencesGetDefaultFontSize(const void* preferencesRef) {
    return FontSize(PreferencesFromRef(preferencesRef, __func__)->GetDouble(u"DefaultFontSize"));
}

void APS5_VABI WKPreferencesSetDefaultFixedFontSize(const void* preferencesRef, std::uint32_t size) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DefaultFixedFontSize", static_cast<double>(size));
}

std::uint32_t APS5_VABI WKPreferencesGetDefaultFixedFontSize(const void* preferencesRef) {
    return FontSize(PreferencesFromRef(preferencesRef, __func__)->GetDouble(u"DefaultFixedFontSize"));
}

void APS5_VABI WKPreferencesSetMinimumFontSize(const void* preferencesRef, std::uint32_t size) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MinimumFontSize", static_cast<double>(size));
}

std::uint32_t APS5_VABI WKPreferencesGetMinimumFontSize(const void* preferencesRef) {
    return FontSize(PreferencesFromRef(preferencesRef, __func__)->GetDouble(u"MinimumFontSize"));
}

void APS5_VABI WKPreferencesSetCookieEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CookieEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetCookieEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CookieEnabled");
}

void APS5_VABI WKPreferencesSetEditableLinkBehavior(const void* preferencesRef, WKEditableLinkBehavior wkBehavior) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"EditableLinkBehavior", static_cast<std::uint32_t>(wkBehavior >= 0 && wkBehavior <= 4 ? wkBehavior : kWKEditableLinkBehaviorNeverLive));
}

WKEditableLinkBehavior APS5_VABI WKPreferencesGetEditableLinkBehavior(const void* preferencesRef) {
    const auto value = PreferencesFromRef(preferencesRef, __func__)->GetUInt32(u"EditableLinkBehavior");
    return static_cast<WKEditableLinkBehavior>(value <= 4 ? value : 4);
}

void APS5_VABI WKPreferencesSetDefaultTextEncodingName(const void* preferencesRef, const void* name) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DefaultTextEncodingName", StringFromRef(name, __func__));
}

const void* APS5_VABI WKPreferencesCopyDefaultTextEncodingName(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"DefaultTextEncodingName").value_or(std::u16string())));
}

void APS5_VABI WKPreferencesSetDeveloperExtrasEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DeveloperExtrasEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetDeveloperExtrasEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"DeveloperExtrasEnabled");
}

void APS5_VABI WKPreferencesSetJavaScriptRuntimeFlags(const void* preferencesRef, std::uint32_t javaScriptRuntimeFlagSet) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"JavaScriptRuntimeFlags", static_cast<std::uint32_t>(javaScriptRuntimeFlagSet));
}

std::uint32_t APS5_VABI WKPreferencesGetJavaScriptRuntimeFlags(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetUInt32(u"JavaScriptRuntimeFlags");
}

void APS5_VABI WKPreferencesSetTextAreasAreResizable(const void* preferencesRef, bool resizable) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"TextAreasAreResizable", resizable);
}

bool APS5_VABI WKPreferencesGetTextAreasAreResizable(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"TextAreasAreResizable");
}

void APS5_VABI WKPreferencesSetSubpixelAntialiasedLayerTextEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetSubpixelAntialiasedLayerTextEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetAcceleratedDrawingEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AcceleratedDrawingEnabled", flag);
}

bool APS5_VABI WKPreferencesGetAcceleratedDrawingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AcceleratedDrawingEnabled");
}

void APS5_VABI WKPreferencesSetCanvasUsesAcceleratedDrawing(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CanvasUsesAcceleratedDrawing", flag);
}

bool APS5_VABI WKPreferencesGetCanvasUsesAcceleratedDrawing(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CanvasUsesAcceleratedDrawing");
}

void APS5_VABI WKPreferencesSetAcceleratedCompositingEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AcceleratedCompositingEnabled", flag);
}

bool APS5_VABI WKPreferencesGetAcceleratedCompositingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AcceleratedCompositingEnabled");
}

void APS5_VABI WKPreferencesSetCompositingBordersVisible(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CompositingBordersVisible", flag);
}

bool APS5_VABI WKPreferencesGetCompositingBordersVisible(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CompositingBordersVisible");
}

void APS5_VABI WKPreferencesSetCompositingRepaintCountersVisible(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CompositingRepaintCountersVisible", flag);
}

bool APS5_VABI WKPreferencesGetCompositingRepaintCountersVisible(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CompositingRepaintCountersVisible");
}

void APS5_VABI WKPreferencesSetTiledScrollingIndicatorVisible(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"TiledScrollingIndicatorVisible", flag);
}

bool APS5_VABI WKPreferencesGetTiledScrollingIndicatorVisible(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"TiledScrollingIndicatorVisible");
}

void APS5_VABI WKPreferencesSetWebGLEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"WebGLEnabled", flag);
}

bool APS5_VABI WKPreferencesGetWebGLEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"WebGLEnabled");
}

void APS5_VABI WKPreferencesSetNeedsSiteSpecificQuirks(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"NeedsSiteSpecificQuirks", flag);
}

bool APS5_VABI WKPreferencesGetNeedsSiteSpecificQuirks(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"NeedsSiteSpecificQuirks");
}

void APS5_VABI WKPreferencesSetForceFTPDirectoryListings(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ForceFTPDirectoryListings", flag);
}

bool APS5_VABI WKPreferencesGetForceFTPDirectoryListings(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ForceFTPDirectoryListings");
}

void APS5_VABI WKPreferencesSetFTPDirectoryTemplatePath(const void* preferencesRef, const void* pathRef) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"FTPDirectoryTemplatePath", StringFromRef(pathRef, __func__));
}

const void* APS5_VABI WKPreferencesCopyFTPDirectoryTemplatePath(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"FTPDirectoryTemplatePath").value_or(std::u16string())));
}

void APS5_VABI WKPreferencesSetTabsToLinks(const void* preferencesRef, bool tabsToLinks) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"TabsToLinks", tabsToLinks);
}

bool APS5_VABI WKPreferencesGetTabsToLinks(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"TabsToLinks");
}

void APS5_VABI WKPreferencesSetDNSPrefetchingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DNSPrefetchingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetDNSPrefetchingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"DNSPrefetchingEnabled");
}

void APS5_VABI WKPreferencesSetAuthorAndUserStylesEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AuthorAndUserStylesEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetAuthorAndUserStylesEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AuthorAndUserStylesEnabled");
}

void APS5_VABI WKPreferencesSetShouldPrintBackgrounds(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShouldPrintBackgrounds", flag);
}

bool APS5_VABI WKPreferencesGetShouldPrintBackgrounds(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShouldPrintBackgrounds");
}

void APS5_VABI WKPreferencesSetDOMTimersThrottlingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DOMTimersThrottlingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetDOMTimersThrottlingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"DOMTimersThrottlingEnabled");
}

void APS5_VABI WKPreferencesSetWebArchiveDebugModeEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"WebArchiveDebugModeEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetWebArchiveDebugModeEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"WebArchiveDebugModeEnabled");
}

void APS5_VABI WKPreferencesSetLocalFileContentSniffingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"LocalFileContentSniffingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetLocalFileContentSniffingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"LocalFileContentSniffingEnabled");
}

void APS5_VABI WKPreferencesSetPageCacheEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"UsesBackForwardCache", enabled);
}

bool APS5_VABI WKPreferencesGetPageCacheEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"UsesBackForwardCache");
}

void APS5_VABI WKPreferencesSetPageCacheSupportsPlugins(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetPageCacheSupportsPlugins(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetDOMPasteAllowed(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DOMPasteAllowed", enabled);
}

bool APS5_VABI WKPreferencesGetDOMPasteAllowed(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"DOMPasteAllowed");
}

void APS5_VABI WKPreferencesSetJavaScriptCanAccessClipboard(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"JavaScriptCanAccessClipboard", enabled);
}

bool APS5_VABI WKPreferencesGetJavaScriptCanAccessClipboard(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"JavaScriptCanAccessClipboard");
}

void APS5_VABI WKPreferencesSetFullScreenEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"FullScreenEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetFullScreenEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"FullScreenEnabled");
}

void APS5_VABI WKPreferencesSetAsynchronousSpellCheckingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AsynchronousSpellCheckingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetAsynchronousSpellCheckingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AsynchronousSpellCheckingEnabled");
}

void APS5_VABI WKPreferencesSetAVFoundationEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AVFoundationEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetAVFoundationEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AVFoundationEnabled");
}

void APS5_VABI WKPreferencesSetWebSecurityEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"WebSecurityEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetWebSecurityEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"WebSecurityEnabled");
}

void APS5_VABI WKPreferencesSetUniversalAccessFromFileURLsAllowed(const void* preferencesRef, bool allowed) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AllowUniversalAccessFromFileURLs", allowed);
}

bool APS5_VABI WKPreferencesGetUniversalAccessFromFileURLsAllowed(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AllowUniversalAccessFromFileURLs");
}

void APS5_VABI WKPreferencesSetFileAccessFromFileURLsAllowed(const void* preferencesRef, bool allowed) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AllowFileAccessFromFileURLs", allowed);
}

bool APS5_VABI WKPreferencesGetFileAccessFromFileURLsAllowed(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AllowFileAccessFromFileURLs");
}

void APS5_VABI WKPreferencesSetTopNavigationToDataURLsAllowed(const void* preferencesRef, bool allowed) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AllowTopNavigationToDataURLs", allowed);
}

bool APS5_VABI WKPreferencesGetTopNavigationToDataURLsAllowed(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AllowTopNavigationToDataURLs");
}

void APS5_VABI WKPreferencesSetNeedsStorageAccessFromFileURLsQuirk(const void* preferencesRef, bool needsQuirk) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"NeedsStorageAccessFromFileURLsQuirk", needsQuirk);
}

bool APS5_VABI WKPreferencesGetNeedsStorageAccessFromFileURLsQuirk(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"NeedsStorageAccessFromFileURLsQuirk");
}

void APS5_VABI WKPreferencesSetMediaPlaybackRequiresUserGesture(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"RequiresUserGestureForMediaPlayback", flag);
}

bool APS5_VABI WKPreferencesGetMediaPlaybackRequiresUserGesture(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"RequiresUserGestureForMediaPlayback");
}

void APS5_VABI WKPreferencesSetVideoPlaybackRequiresUserGesture(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"RequiresUserGestureForVideoPlayback", flag);
}

bool APS5_VABI WKPreferencesGetVideoPlaybackRequiresUserGesture(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"RequiresUserGestureForVideoPlayback");
}

void APS5_VABI WKPreferencesSetAudioPlaybackRequiresUserGesture(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"RequiresUserGestureForAudioPlayback", flag);
}

bool APS5_VABI WKPreferencesGetAudioPlaybackRequiresUserGesture(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"RequiresUserGestureForAudioPlayback");
}

void APS5_VABI WKPreferencesSetMainContentUserGestureOverrideEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MainContentUserGestureOverrideEnabled", flag);
}

bool APS5_VABI WKPreferencesGetMainContentUserGestureOverrideEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MainContentUserGestureOverrideEnabled");
}

void APS5_VABI WKPreferencesSetMediaPlaybackAllowsInline(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AllowsInlineMediaPlayback", flag);
}

bool APS5_VABI WKPreferencesGetMediaPlaybackAllowsInline(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AllowsInlineMediaPlayback");
}

void APS5_VABI WKPreferencesSetInlineMediaPlaybackRequiresPlaysInlineAttribute(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"InlineMediaPlaybackRequiresPlaysInlineAttribute", flag);
}

bool APS5_VABI WKPreferencesGetInlineMediaPlaybackRequiresPlaysInlineAttribute(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"InlineMediaPlaybackRequiresPlaysInlineAttribute");
}

void APS5_VABI WKPreferencesSetBeaconAPIEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"BeaconAPIEnabled", flag);
}

bool APS5_VABI WKPreferencesGetBeaconAPIEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"BeaconAPIEnabled");
}

void APS5_VABI WKPreferencesSetDirectoryUploadEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DirectoryUploadEnabled", flag);
}

bool APS5_VABI WKPreferencesGetDirectoryUploadEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"DirectoryUploadEnabled");
}

void APS5_VABI WKPreferencesSetMediaControlsScaleWithPageZoom(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaControlsScaleWithPageZoom", flag);
}

bool APS5_VABI WKPreferencesGetMediaControlsScaleWithPageZoom(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MediaControlsScaleWithPageZoom");
}

void APS5_VABI WKPreferencesSetWebAuthenticationEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"WebAuthenticationEnabled", flag);
}

bool APS5_VABI WKPreferencesGetWebAuthenticationEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"WebAuthenticationEnabled");
}

void APS5_VABI WKPreferencesSetInvisibleMediaAutoplayPermitted(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"InvisibleAutoplayNotPermitted", !flag);
}

bool APS5_VABI WKPreferencesGetInvisibleMediaAutoplayPermitted(const void* preferencesRef) {
    return !PreferencesFromRef(preferencesRef, __func__)->GetBool(u"InvisibleAutoplayNotPermitted");
}

void APS5_VABI WKPreferencesSetShowsToolTipOverTruncatedText(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShowsToolTipOverTruncatedText", flag);
}

bool APS5_VABI WKPreferencesGetShowsToolTipOverTruncatedText(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShowsToolTipOverTruncatedText");
}

void APS5_VABI WKPreferencesSetMockScrollbarsEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MockScrollbarsEnabled", flag);
}

bool APS5_VABI WKPreferencesGetMockScrollbarsEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MockScrollbarsEnabled");
}

void APS5_VABI WKPreferencesSetAttachmentElementEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AttachmentElementEnabled", flag);
}

bool APS5_VABI WKPreferencesGetAttachmentElementEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AttachmentElementEnabled");
}

void APS5_VABI WKPreferencesSetWebAudioEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"WebAudioEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetWebAudioEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"WebAudioEnabled");
}

void APS5_VABI WKPreferencesSetSuppressesIncrementalRendering(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"SuppressesIncrementalRendering", enabled);
}

bool APS5_VABI WKPreferencesGetSuppressesIncrementalRendering(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"SuppressesIncrementalRendering");
}

void APS5_VABI WKPreferencesSetBackspaceKeyNavigationEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"BackspaceKeyNavigationEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetBackspaceKeyNavigationEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"BackspaceKeyNavigationEnabled");
}

void APS5_VABI WKPreferencesSetCaretBrowsingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CaretBrowsingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetCaretBrowsingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CaretBrowsingEnabled");
}

void APS5_VABI WKPreferencesSetShouldDisplaySubtitles(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShouldDisplaySubtitles", enabled);
}

bool APS5_VABI WKPreferencesGetShouldDisplaySubtitles(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShouldDisplaySubtitles");
}

void APS5_VABI WKPreferencesSetShouldDisplayCaptions(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShouldDisplayCaptions", enabled);
}

bool APS5_VABI WKPreferencesGetShouldDisplayCaptions(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShouldDisplayCaptions");
}

void APS5_VABI WKPreferencesSetShouldDisplayTextDescriptions(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShouldDisplayTextDescriptions", enabled);
}

bool APS5_VABI WKPreferencesGetShouldDisplayTextDescriptions(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShouldDisplayTextDescriptions");
}

void APS5_VABI WKPreferencesSetNotificationsEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"NotificationsEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetNotificationsEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"NotificationsEnabled");
}

void APS5_VABI WKPreferencesSetShouldRespectImageOrientation(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShouldRespectImageOrientation", enabled);
}

bool APS5_VABI WKPreferencesGetShouldRespectImageOrientation(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShouldRespectImageOrientation");
}

void APS5_VABI WKPreferencesSetRequestAnimationFrameEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetRequestAnimationFrameEnabled(const void*) {
    return true;
}

void APS5_VABI WKPreferencesSetStorageBlockingPolicy(const void* preferencesRef, WKStorageBlockingPolicy policy) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"StorageBlockingPolicy", static_cast<std::uint32_t>(policy >= 0 && policy <= 2 ? policy : kWKAllowAllStorage));
}

WKStorageBlockingPolicy APS5_VABI WKPreferencesGetStorageBlockingPolicy(const void* preferencesRef) {
    const auto value = PreferencesFromRef(preferencesRef, __func__)->GetUInt32(u"StorageBlockingPolicy");
    return static_cast<WKStorageBlockingPolicy>(value <= 2 ? value : 0);
}

void APS5_VABI WKPreferencesSetDiagnosticLoggingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DiagnosticLoggingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetDiagnosticLoggingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"DiagnosticLoggingEnabled");
}

void APS5_VABI WKPreferencesSetAsynchronousPluginInitializationEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetAsynchronousPluginInitializationEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetAsynchronousPluginInitializationEnabledForAllPlugins(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetAsynchronousPluginInitializationEnabledForAllPlugins(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetArtificialPluginInitializationDelayEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetArtificialPluginInitializationDelayEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetInteractiveFormValidationEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"InteractiveFormValidationEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetInteractiveFormValidationEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"InteractiveFormValidationEnabled");
}

void APS5_VABI WKPreferencesSetScrollingPerformanceLoggingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ScrollingPerformanceTestingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetScrollingPerformanceLoggingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ScrollingPerformanceTestingEnabled");
}

void APS5_VABI WKPreferencesSetPlugInSnapshottingEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetPlugInSnapshottingEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetPluginSandboxProfilesEnabledForAllPlugins(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetPluginSandboxProfilesEnabledForAllPlugins(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetSnapshotAllPlugIns(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetSnapshotAllPlugIns(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetAutostartOriginPlugInSnapshottingEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetAutostartOriginPlugInSnapshottingEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetPrimaryPlugInSnapshotDetectionEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetPrimaryPlugInSnapshotDetectionEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetPDFPluginEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"PDFPluginEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetPDFPluginEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"PDFPluginEnabled");
}

void APS5_VABI WKPreferencesSetEncodingDetectorEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"UsesEncodingDetector", enabled);
}

bool APS5_VABI WKPreferencesGetEncodingDetectorEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"UsesEncodingDetector");
}

void APS5_VABI WKPreferencesSetTextAutosizingEnabled(const void* preferencesRef, bool textAutosizingEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"TextAutosizingEnabled", textAutosizingEnabled);
}

bool APS5_VABI WKPreferencesGetTextAutosizingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"TextAutosizingEnabled");
}

void APS5_VABI WKPreferencesSetTextAutosizingUsesIdempotentMode(const void* preferencesRef, bool textAutosizingUsesIdempotentModeEnabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"TextAutosizingUsesIdempotentMode", textAutosizingUsesIdempotentModeEnabled);
}

bool APS5_VABI WKPreferencesGetTextAutosizingUsesIdempotentMode(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"TextAutosizingUsesIdempotentMode");
}

void APS5_VABI WKPreferencesSetAggressiveTileRetentionEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AggressiveTileRetentionEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetAggressiveTileRetentionEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AggressiveTileRetentionEnabled");
}

void APS5_VABI WKPreferencesSetLogsPageMessagesToSystemConsoleEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"LogsPageMessagesToSystemConsoleEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetLogsPageMessagesToSystemConsoleEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"LogsPageMessagesToSystemConsoleEnabled");
}

void APS5_VABI WKPreferencesSetPageVisibilityBasedProcessSuppressionEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"PageVisibilityBasedProcessSuppressionEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetPageVisibilityBasedProcessSuppressionEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"PageVisibilityBasedProcessSuppressionEnabled");
}

void APS5_VABI WKPreferencesSetSmartInsertDeleteEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"SmartInsertDeleteEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetSmartInsertDeleteEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"SmartInsertDeleteEnabled");
}

void APS5_VABI WKPreferencesSetSelectTrailingWhitespaceEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"SelectTrailingWhitespaceEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetSelectTrailingWhitespaceEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"SelectTrailingWhitespaceEnabled");
}

void APS5_VABI WKPreferencesSetShowsURLsInToolTipsEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShowsURLsInToolTipsEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetShowsURLsInToolTipsEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShowsURLsInToolTipsEnabled");
}

void APS5_VABI WKPreferencesSetHiddenPageDOMTimerThrottlingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"HiddenPageDOMTimerThrottlingEnabled", enabled);
}

void APS5_VABI WKPreferencesSetHiddenPageDOMTimerThrottlingAutoIncreases(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"HiddenPageDOMTimerThrottlingAutoIncreases", enabled);
}

bool APS5_VABI WKPreferencesGetHiddenPageDOMTimerThrottlingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"HiddenPageDOMTimerThrottlingEnabled");
}

bool APS5_VABI WKPreferencesGetHiddenPageDOMTimerThrottlingAutoIncreases(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"HiddenPageDOMTimerThrottlingAutoIncreases");
}

void APS5_VABI WKPreferencesSetHiddenPageCSSAnimationSuspensionEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"HiddenPageCSSAnimationSuspensionEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetHiddenPageCSSAnimationSuspensionEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"HiddenPageCSSAnimationSuspensionEnabled");
}

void APS5_VABI WKPreferencesSetIncrementalRenderingSuppressionTimeout(const void* preferencesRef, double timeout) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"IncrementalRenderingSuppressionTimeout", static_cast<double>(timeout));
}

double APS5_VABI WKPreferencesGetIncrementalRenderingSuppressionTimeout(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetDouble(u"IncrementalRenderingSuppressionTimeout");
}

void APS5_VABI WKPreferencesSetThreadedScrollingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ThreadedScrollingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetThreadedScrollingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ThreadedScrollingEnabled");
}

void APS5_VABI WKPreferencesSetContentChangeObserverEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ContentChangeObserverEnabled", flag);
}

bool APS5_VABI WKPreferencesGetContentChangeObserverEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ContentChangeObserverEnabled");
}

void APS5_VABI WKPreferencesSetUseGiantTiles(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"UseGiantTiles", flag);
}

bool APS5_VABI WKPreferencesGetUseGiantTiles(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"UseGiantTiles");
}

void APS5_VABI WKPreferencesSetMediaDevicesEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaDevicesEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetMediaDevicesEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MediaDevicesEnabled");
}

void APS5_VABI WKPreferencesSetMediaStreamEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaStreamEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetMediaStreamEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MediaStreamEnabled");
}

void APS5_VABI WKPreferencesSetPeerConnectionEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"PeerConnectionEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetPeerConnectionEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"PeerConnectionEnabled");
}

void APS5_VABI WKPreferencesSetSpatialNavigationEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"SpatialNavigationEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetSpatialNavigationEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"SpatialNavigationEnabled");
}

void APS5_VABI WKPreferencesSetMediaSourceEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaSourceEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetMediaSourceEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MediaSourceEnabled");
}

void APS5_VABI WKPreferencesSetSourceBufferChangeTypeEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"SourceBufferChangeTypeEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetSourceBufferChangeTypeEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"SourceBufferChangeTypeEnabled");
}

void APS5_VABI WKPreferencesSetViewGestureDebuggingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ViewGestureDebuggingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetViewGestureDebuggingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ViewGestureDebuggingEnabled");
}

void APS5_VABI WKPreferencesSetShouldConvertPositionStyleOnCopy(const void* preferencesRef, bool convert) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShouldConvertPositionStyleOnCopy", convert);
}

bool APS5_VABI WKPreferencesGetShouldConvertPositionStyleOnCopy(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShouldConvertPositionStyleOnCopy");
}

void APS5_VABI WKPreferencesSetTelephoneNumberParsingEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"TelephoneNumberParsingEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetTelephoneNumberParsingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"TelephoneNumberParsingEnabled");
}

void APS5_VABI WKPreferencesSetEnableInheritURIQueryComponent(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"EnableInheritURIQueryComponent", enabled);
}

bool APS5_VABI WKPreferencesGetEnableInheritURIQueryComponent(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"EnableInheritURIQueryComponent");
}

void APS5_VABI WKPreferencesSetServiceControlsEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ServiceControlsEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetServiceControlsEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ServiceControlsEnabled");
}

void APS5_VABI WKPreferencesSetImageControlsEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ImageControlsEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetImageControlsEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ImageControlsEnabled");
}

void APS5_VABI WKPreferencesSetGamepadsEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"GamepadsEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetGamepadsEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"GamepadsEnabled");
}

void APS5_VABI WKPreferencesSetHighlightAPIEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"HighlightAPIEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetHighlightAPIEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"HighlightAPIEnabled");
}

void APS5_VABI WKPreferencesSetMinimumZoomFontSize(const void* preferencesRef, double size) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MinimumZoomFontSize", static_cast<double>(size));
}

double APS5_VABI WKPreferencesGetMinimumZoomFontSize(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetDouble(u"MinimumZoomFontSize");
}

void APS5_VABI WKPreferencesSetVisibleDebugOverlayRegions(const void* preferencesRef, std::uint32_t visibleRegions) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"VisibleDebugOverlayRegions", static_cast<std::uint32_t>(visibleRegions));
}

std::uint32_t APS5_VABI WKPreferencesGetVisibleDebugOverlayRegions(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetUInt32(u"VisibleDebugOverlayRegions");
}

void APS5_VABI WKPreferencesSetMetaRefreshEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"HTTPEquivEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetMetaRefreshEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"HTTPEquivEnabled");
}

void APS5_VABI WKPreferencesSetHTTPEquivEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"HTTPEquivEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetHTTPEquivEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"HTTPEquivEnabled");
}

void APS5_VABI WKPreferencesSetAllowsAirPlayForMediaPlayback(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AllowsAirPlayForMediaPlayback", enabled);
}

bool APS5_VABI WKPreferencesGetAllowsAirPlayForMediaPlayback(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AllowsAirPlayForMediaPlayback");
}

void APS5_VABI WKPreferencesSetUserInterfaceDirectionPolicy(const void* preferencesRef, _WKUserInterfaceDirectionPolicy userInterfaceDirectionPolicy) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"UserInterfaceDirectionPolicy", static_cast<std::uint32_t>(userInterfaceDirectionPolicy));
}

_WKUserInterfaceDirectionPolicy APS5_VABI WKPreferencesGetUserInterfaceDirectionPolicy(const void* preferencesRef) {
    return static_cast<_WKUserInterfaceDirectionPolicy>(PreferencesFromRef(preferencesRef, __func__)->GetUInt32(u"UserInterfaceDirectionPolicy"));
}

void APS5_VABI WKPreferencesSetResourceUsageOverlayVisible(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ResourceUsageOverlayVisible", enabled);
}

bool APS5_VABI WKPreferencesGetResourceUsageOverlayVisible(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ResourceUsageOverlayVisible");
}

void APS5_VABI WKPreferencesSetMockCaptureDevicesEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MockCaptureDevicesEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetMockCaptureDevicesEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MockCaptureDevicesEnabled");
}

void APS5_VABI WKPreferencesSetICECandidateFilteringEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ICECandidateFilteringEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetICECandidateFilteringEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ICECandidateFilteringEnabled");
}

void APS5_VABI WKPreferencesSetEnumeratingAllNetworkInterfacesEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"EnumeratingAllNetworkInterfacesEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetEnumeratingAllNetworkInterfacesEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"EnumeratingAllNetworkInterfacesEnabled");
}

void APS5_VABI WKPreferencesSetMediaCaptureRequiresSecureConnection(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaCaptureRequiresSecureConnection", enabled);
}

bool APS5_VABI WKPreferencesGetMediaCaptureRequiresSecureConnection(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MediaCaptureRequiresSecureConnection");
}

void APS5_VABI WKPreferencesSetInactiveMediaCaptureSteamRepromptIntervalInMinutes(const void* preferencesRef, double interval) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"InactiveMediaCaptureSteamRepromptIntervalInMinutes", static_cast<double>(interval));
}

double APS5_VABI WKPreferencesGetInactiveMediaCaptureSteamRepromptIntervalInMinutes(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetDouble(u"InactiveMediaCaptureSteamRepromptIntervalInMinutes");
}

void APS5_VABI WKPreferencesSetDataTransferItemsEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DataTransferItemsEnabled", flag);
}

bool APS5_VABI WKPreferencesGetDataTransferItemsEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"DataTransferItemsEnabled");
}

void APS5_VABI WKPreferencesSetCustomPasteboardDataEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CustomPasteboardDataEnabled", flag);
}

bool APS5_VABI WKPreferencesGetCustomPasteboardDataEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CustomPasteboardDataEnabled");
}

void APS5_VABI WKPreferencesSetWebShareEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"WebShareEnabled", flag);
}

bool APS5_VABI WKPreferencesGetWebShareEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"WebShareEnabled");
}

void APS5_VABI WKPreferencesSetDownloadAttributeEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"DownloadAttributeEnabled", flag);
}

bool APS5_VABI WKPreferencesGetDownloadAttributeEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"DownloadAttributeEnabled");
}

void APS5_VABI WKPreferencesSetIntersectionObserverEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"IntersectionObserverEnabled", flag);
}

bool APS5_VABI WKPreferencesGetIntersectionObserverEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"IntersectionObserverEnabled");
}

void APS5_VABI WKPreferencesSetMenuItemElementEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MenuItemElementEnabled", flag);
}

bool APS5_VABI WKPreferencesGetMenuItemElementEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MenuItemElementEnabled");
}

void APS5_VABI WKPreferencesSetPaintTimingEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"PaintTimingEnabled", flag);
}

bool APS5_VABI WKPreferencesGetPaintTimingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"PaintTimingEnabled");
}

void APS5_VABI WKPreferencesSetIsAccessibilityIsolatedTreeEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"IsAccessibilityIsolatedTreeEnabled", flag);
}

bool APS5_VABI WKPreferencesGetIsAccessibilityIsolatedTreeEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"IsAccessibilityIsolatedTreeEnabled");
}

void APS5_VABI WKPreferencesSetFetchAPIKeepAliveEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"FetchAPIKeepAliveEnabled", flag);
}

bool APS5_VABI WKPreferencesGetFetchAPIKeepAliveEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"FetchAPIKeepAliveEnabled");
}

void APS5_VABI WKPreferencesSetAllowsPictureInPictureMediaPlayback(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AllowsPictureInPictureMediaPlayback", enabled);
}

bool APS5_VABI WKPreferencesGetAllowsPictureInPictureMediaPlayback(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AllowsPictureInPictureMediaPlayback");
}

bool APS5_VABI WKPreferencesGetApplePayEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ApplePayEnabled");
}

void APS5_VABI WKPreferencesSetApplePayEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ApplePayEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetApplePayCapabilityDisclosureAllowed(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ApplePayCapabilityDisclosureAllowed");
}

void APS5_VABI WKPreferencesSetApplePayCapabilityDisclosureAllowed(const void* preferencesRef, bool allowed) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ApplePayCapabilityDisclosureAllowed", allowed);
}

void APS5_VABI WKPreferencesSetLinkPreloadEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"LinkPreloadEnabled", flag);
}

bool APS5_VABI WKPreferencesGetLinkPreloadEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"LinkPreloadEnabled");
}

void APS5_VABI WKPreferencesSetMediaPreloadingEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaPreloadingEnabled", flag);
}

bool APS5_VABI WKPreferencesGetMediaPreloadingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MediaPreloadingEnabled");
}

void APS5_VABI WKPreferencesSetLargeImageAsyncDecodingEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"LargeImageAsyncDecodingEnabled", flag);
}

bool APS5_VABI WKPreferencesGetLargeImageAsyncDecodingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"LargeImageAsyncDecodingEnabled");
}

void APS5_VABI WKPreferencesSetAnimatedImageAsyncDecodingEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AnimatedImageAsyncDecodingEnabled", flag);
}

bool APS5_VABI WKPreferencesGetAnimatedImageAsyncDecodingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AnimatedImageAsyncDecodingEnabled");
}

void APS5_VABI WKPreferencesSetEncryptedMediaAPIEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"EncryptedMediaAPIEnabled", flag);
}

bool APS5_VABI WKPreferencesGetEncryptedMediaAPIEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"EncryptedMediaAPIEnabled");
}

void APS5_VABI WKPreferencesSetShouldSuppressKeyboardInputDuringProvisionalNavigation(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShouldSuppressTextInputFromEditingDuringProvisionalNavigation", flag);
}

bool APS5_VABI WKPreferencesGetShouldSuppressKeyboardInputDuringProvisionalNavigation(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShouldSuppressTextInputFromEditingDuringProvisionalNavigation");
}

void APS5_VABI WKPreferencesSetMediaUserGestureInheritsFromDocument(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaUserGestureInheritsFromDocument", flag);
}

bool APS5_VABI WKPreferencesGetMediaUserGestureInheritsFromDocument(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MediaUserGestureInheritsFromDocument");
}

void APS5_VABI WKPreferencesSetMediaContentTypesRequiringHardwareSupport(const void* preferencesRef, const void* codecs) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaContentTypesRequiringHardwareSupport", StringFromRef(codecs, __func__));
}

const void* APS5_VABI WKPreferencesCopyMediaContentTypesRequiringHardwareSupport(const void* preferencesRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(PreferencesFromRef(preferencesRef, __func__)->GetString(u"MediaContentTypesRequiringHardwareSupport").value_or(std::u16string())));
}

bool APS5_VABI WKPreferencesGetLegacyEncryptedMediaAPIEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"LegacyEncryptedMediaAPIEnabled");
}

void APS5_VABI WKPreferencesSetLegacyEncryptedMediaAPIEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"LegacyEncryptedMediaAPIEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetAllowMediaContentTypesRequiringHardwareSupportAsFallback(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AllowMediaContentTypesRequiringHardwareSupportAsFallback");
}

void APS5_VABI WKPreferencesSetAllowMediaContentTypesRequiringHardwareSupportAsFallback(const void* preferencesRef, bool allow) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AllowMediaContentTypesRequiringHardwareSupportAsFallback", allow);
}

void APS5_VABI WKPreferencesSetStorageAccessAPIEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"StorageAccessAPIEnabled", flag);
}

bool APS5_VABI WKPreferencesGetStorageAccessAPIEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"StorageAccessAPIEnabled");
}

void APS5_VABI WKPreferencesSetSyntheticEditingCommandsEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"SyntheticEditingCommandsEnabled", flag);
}

bool APS5_VABI WKPreferencesGetSyntheticEditingCommandsEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"SyntheticEditingCommandsEnabled");
}

void APS5_VABI WKPreferencesSetCSSOMViewScrollingAPIEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CSSOMViewScrollingAPIEnabled", flag);
}

bool APS5_VABI WKPreferencesGetCSSOMViewScrollingAPIEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CSSOMViewScrollingAPIEnabled");
}

void APS5_VABI WKPreferencesSetShouldAllowUserInstalledFonts(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShouldAllowUserInstalledFonts", flag);
}

bool APS5_VABI WKPreferencesGetShouldAllowUserInstalledFonts(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShouldAllowUserInstalledFonts");
}

void APS5_VABI WKPreferencesSetMediaCapabilitiesEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaCapabilitiesEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetMediaCapabilitiesEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MediaCapabilitiesEnabled");
}

void APS5_VABI WKPreferencesSetAllowCrossOriginSubresourcesToAskForCredentials(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"AllowCrossOriginSubresourcesToAskForCredentials", flag);
}

bool APS5_VABI WKPreferencesGetAllowCrossOriginSubresourcesToAskForCredentials(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"AllowCrossOriginSubresourcesToAskForCredentials");
}

void APS5_VABI WKPreferencesSetRestrictedHTTPResponseAccess(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"RestrictedHTTPResponseAccess", flag);
}

bool APS5_VABI WKPreferencesGetRestrictedHTTPResponseAccess(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"RestrictedHTTPResponseAccess");
}

void APS5_VABI WKPreferencesSetServerTimingEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ServerTimingEnabled", flag);
}

bool APS5_VABI WKPreferencesGetServerTimingEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ServerTimingEnabled");
}

void APS5_VABI WKPreferencesSetColorFilterEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ColorFilterEnabled", flag);
}

bool APS5_VABI WKPreferencesGetColorFilterEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ColorFilterEnabled");
}

void APS5_VABI WKPreferencesSetProcessSwapOnNavigationEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ProcessSwapOnCrossSiteNavigationEnabled", flag);
}

bool APS5_VABI WKPreferencesGetProcessSwapOnNavigationEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ProcessSwapOnCrossSiteNavigationEnabled");
}

void APS5_VABI WKPreferencesSetPunchOutWhiteBackgroundsInDarkMode(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"PunchOutWhiteBackgroundsInDarkMode", flag);
}

bool APS5_VABI WKPreferencesGetPunchOutWhiteBackgroundsInDarkMode(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"PunchOutWhiteBackgroundsInDarkMode");
}

void APS5_VABI WKPreferencesSetCaptureAudioInUIProcessEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CaptureAudioInUIProcessEnabled", flag);
}

bool APS5_VABI WKPreferencesGetCaptureAudioInUIProcessEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CaptureAudioInUIProcessEnabled");
}

void APS5_VABI WKPreferencesSetCaptureAudioInGPUProcessEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CaptureAudioInGPUProcessEnabled", flag);
}

bool APS5_VABI WKPreferencesGetCaptureAudioInGPUProcessEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CaptureAudioInGPUProcessEnabled");
}

void APS5_VABI WKPreferencesSetCaptureVideoInUIProcessEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CaptureVideoInUIProcessEnabled", flag);
}

bool APS5_VABI WKPreferencesGetCaptureVideoInUIProcessEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CaptureVideoInUIProcessEnabled");
}

void APS5_VABI WKPreferencesSetCaptureVideoInGPUProcessEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"CaptureVideoInGPUProcessEnabled", flag);
}

bool APS5_VABI WKPreferencesGetCaptureVideoInGPUProcessEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"CaptureVideoInGPUProcessEnabled");
}

void APS5_VABI WKPreferencesSetReferrerPolicyAttributeEnabled(const void* preferencesRef, bool flag) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ReferrerPolicyAttributeEnabled", flag);
}

bool APS5_VABI WKPreferencesGetReferrerPolicyAttributeEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ReferrerPolicyAttributeEnabled");
}

bool APS5_VABI WKPreferencesGetRemotePlaybackEnabled(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"RemotePlaybackEnabled");
}

void APS5_VABI WKPreferencesSetRemotePlaybackEnabled(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"RemotePlaybackEnabled", enabled);
}

bool APS5_VABI WKPreferencesGetShouldUseServiceWorkerShortTimeout(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"ShouldUseServiceWorkerShortTimeout");
}

void APS5_VABI WKPreferencesSetShouldUseServiceWorkerShortTimeout(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"ShouldUseServiceWorkerShortTimeout", enabled);
}

void APS5_VABI WKPreferencesSetAVFoundationNSURLSessionEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetAVFoundationNSURLSessionEnabled(const void*) {
    return true;
}

void APS5_VABI WKPreferencesSetPrivateBrowsingEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetPrivateBrowsingEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetIgnoreViewportScalingConstraints(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetIgnoreViewportScalingConstraints(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetKeygenElementEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetKeygenElementEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetNewBlockInsideInlineModelEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetNewBlockInsideInlineModelEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetLongMousePressEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetLongMousePressEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetAntialiasedFontDilationEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetAntialiasedFontDilationEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetHixie76WebSocketProtocolEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetHixie76WebSocketProtocolEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetFetchAPIEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetFetchAPIEnabled(const void*) {
    return true;
}

void APS5_VABI WKPreferencesSetIsSecureContextAttributeEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetIsSecureContextAttributeEnabled(const void*) {
    return true;
}

void APS5_VABI WKPreferencesSetUserTimingEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetUserTimingEnabled(const void*) {
    return true;
}

void APS5_VABI WKPreferencesSetResourceTimingEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetResourceTimingEnabled(const void*) {
    return true;
}

void APS5_VABI WKPreferencesSetCrossOriginResourcePolicyEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetCrossOriginResourcePolicyEnabled(const void*) {
    return true;
}

void APS5_VABI WKPreferencesSetSubpixelCSSOMElementMetricsEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetSubpixelCSSOMElementMetricsEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetWebSQLDisabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetWebSQLDisabled(const void*) {
    return true;
}

void APS5_VABI WKPreferencesSetXSSAuditorEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetXSSAuditorEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetJavaEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetJavaEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetPaginateDuringLayoutEnabled(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetPaginateDuringLayoutEnabled(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetLoadsSiteIconsIgnoringImageLoadingPreference(const void*, bool) {
}

bool APS5_VABI WKPreferencesGetLoadsSiteIconsIgnoringImageLoadingPreference(const void*) {
    return false;
}

void APS5_VABI WKPreferencesSetMediaPlayable(const void* preferencesRef, bool enabled) {
    PreferencesFromRef(preferencesRef, __func__)->Set(u"MediaPlayable", enabled);
}

bool APS5_VABI WKPreferencesGetMediaPlayable(const void* preferencesRef) {
    return PreferencesFromRef(preferencesRef, __func__)->GetBool(u"MediaPlayable");
}

}
