#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit constant check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

int main() {
    Require(WKAccessibilityCanUseSecondaryAXThread() == 0);
    Require(WKBundleBackForwardListCopyItemAtIndex() == 0);
    Require(WKBundleBackForwardListGetBackListCount() == 0);
    Require(WKBundleBackForwardListGetForwardListCount() == 0);
    Require(WKBundleBackForwardListItemCopyChildren() == 0);
    Require(WKBundleBackForwardListItemCopyOriginalURL() == 0);
    Require(WKBundleBackForwardListItemCopyTarget() == 0);
    Require(WKBundleBackForwardListItemCopyTitle() == 0);
    Require(WKBundleBackForwardListItemCopyURL() == 0);
    Require(WKBundleBackForwardListItemHasCachedPageExpired() == 0);
    Require(WKBundleBackForwardListItemIsInBackForwardCache() == 0);
    Require(WKBundleBackForwardListItemIsSame() == 0);
    Require(WKBundleBackForwardListItemIsTargetItem() == 0);
    Require(WKBundleFileHandleCreateWithPath() == 0);
    Require(WKBundleFrameCopyWebArchive() == 0);
    Require(WKBundleFrameCopyWebArchiveFilteringSubframes() == 0);
    Require(WKBundleFrameGetJavaScriptWrapperForFileForWorld() == 0);
    Require(WKBundleNodeHandleCopyHTMLFrameElementContentFrame() == 0);
    Require(WKBundleNodeHandleCopyHTMLTableCellElementCellAbove() == 0);
    Require(WKBundleNodeHandleCopyVisibleRange() == 0);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillAvailable() == 0);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillButtonEnabled() == 0);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFilled() == 0);
    Require(WKBundleNodeHandleGetHTMLInputElementAutofilled() == 0);
    Require(WKBundlePageCopyContextMenuAtPointInWindow() == 0);
    Require(WKBundlePageCopyContextMenuItems() == 0);
    Require(WKBundlePageGetBackForwardList() == 0);
    Require(WKContextCopyPlugInAutoStartOriginHashes() == 0);
    Require(WKContextDownloadURLRequest() == 0);
    Require(WKContextGetIconDatabase() == 0);
    Require(WKContextGetMaximumNumberOfProcesses() == static_cast<int>(0xffffffffu));
    Require(WKContextMenuCopySubmenuItems() == 0);
    Require(WKContextMenuItemCopyTitle() == 0);
    Require(WKContextMenuItemCreateAsAction() == 0);
    Require(WKContextMenuItemCreateAsCheckableAction() == 0);
    Require(WKContextMenuItemCreateAsSubmenu() == 0);
    Require(WKContextMenuItemGetChecked() == 0);
    Require(WKContextMenuItemGetEnabled() == 0);
    Require(WKContextMenuItemGetTag() == 0);
    Require(WKContextMenuItemGetType() == 0);
    Require(WKContextMenuItemGetUserData() == 0);
    Require(WKContextMenuItemSeparatorItem() == 0);
    Require(WKContextResumeDownload() == 0);
    Require(WKFrameCreateFrameInfo() == 0);
    Require(WKFrameIsFrameSet() == 0);
    Require(WKIconDatabaseCopyIconDataForPageURL() == 0);
    Require(WKIconDatabaseCopyIconURLForPageURL() == 0);
    Require(WKKeyValueStorageManagerGetCreationTimeKey() == 0);
    Require(WKKeyValueStorageManagerGetModificationTimeKey() == 0);
    Require(WKKeyValueStorageManagerGetOriginKey() == 0);
    Require(WKOpenPanelParametersCopyCapture() == 0);
    Require(WKOpenPanelParametersGetMediaCaptureType() == 0);
    Require(WKPageGetFrameSetLargestFrame() == 0);
    Require(WKPageIsMockRealtimeMediaSourceCenterEnabled() == 0);
    Require(WKPlugInInformationReplacementObscuredKey() == 0);
    Require(WKPluginInformationBundleIdentifierKey() == 0);
    Require(WKPluginInformationBundleShortVersionKey() == 0);
    Require(WKPluginInformationBundleVersionKey() == 0);
    Require(WKPluginInformationDefaultLoadPolicyKey() == 0);
    Require(WKPluginInformationDisplayNameKey() == 0);
    Require(WKPluginInformationFrameURLKey() == 0);
    Require(WKPluginInformationHasSandboxProfileKey() == 0);
    Require(WKPluginInformationMIMETypeKey() == 0);
    Require(WKPluginInformationPageURLKey() == 0);
    Require(WKPluginInformationPathKey() == 0);
    Require(WKPluginInformationPluginURLKey() == 0);
    Require(WKPluginInformationPluginspageAttributeURLKey() == 0);
    Require(WKPluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey() == 0);
    Require(WKPreferencesGetAccelerated2DCanvasEnabled() == 0);
    Require(WKPreferencesGetApplicationChromeModeEnabled() == 0);
    Require(WKPreferencesGetQTKitEnabled() == 0);
    Require(WKPreferencesGetTypeID() == static_cast<int>(0x00000058u));
    Require(WKUserContentExtensionStoreCreate() == 0);
    Require(_ZN3JSC12isFromJSCodeEPv() == 0);
    Require(_ZN3JSC22optimizeNextInvocationENS_7JSValueE() == static_cast<int>(0x0000000au));
    Require(_ZN3JSC22optimizeNextInvocationEPNS_14JSGlobalObjectEPNS_9CallFrameE() == static_cast<int>(0x0000000au));
    Require(_ZN3JSC3DFG18getNumCompilationsEv() == 0);
    Require(_ZN3WTF14FileSystemImpl27isSafeToUseMemoryMapForPathERKNS_6StringE() == 1);
    Require(_ZN3WTF14setCloseOnExecEi() == 1);
    Require(_ZN3WTF15AutomaticThread11shouldSleepERKNS_14AbstractLockerE() == 1);
    Require(_ZN7WebCore10Pasteboard15canSmartReplaceEv() == 0);
    Require(_ZN7WebCore10Pasteboard16fileContentStateEv() == 0);
    Require(_ZN7WebCore12BlobRegistry16blobRegistryImplEv() == 0);
    Require(_ZN7WebCore12EventHandler18accessKeyModifiersEv() == 1);
    Require(_ZN7WebCore13MIMETypeCache21canDecodeExtendedTypeERKNS_11ContentTypeE() == 0);
    Require(_ZN7WebCore13MIMETypeCache26isUnsupportedContainerTypeERKN3WTF6StringE() == 0);
    Require(_ZN7WebCore13getLogChannelERKN3WTF6StringE() == 0);
    Require(_ZN7WebCore16ISOSchemeTypeBox11boxTypeNameEv() == static_cast<int>(0x7363686du));
    Require(_ZN7WebCore17JSDOMGlobalObject21offsetOfWorldIsNormalEv() == static_cast<int>(0x00000930u));
    Require(_ZN7WebCore18PlatformPasteboard5writeERKN3WTF6VectorINS_20PasteboardCustomDataELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE() == 0);
    Require(_ZN7WebCore18PlatformPasteboard5writeERKNS_20PasteboardCustomDataE() == 0);
    Require(_ZN7WebCore20ISOOriginalFormatBox11boxTypeNameEv() == static_cast<int>(0x66726d61u));
    Require(_ZN7WebCore21ISOTrackEncryptionBox11boxTypeNameEv() == static_cast<int>(0x74656e63u));
    Require(_ZN7WebCore21PlatformKeyboardEvent20currentCapsLockStateEv() == 0);
    Require(_ZN7WebCore22EmptyFrameLoaderClient17objectContentTypeERKN3WTF3URLERKNS1_6StringE() == 0);
    Require(_ZN7WebCore22EmptyFrameLoaderClient29hasFrameSpecificStorageAccessEv() == 0);
    Require(_ZN7WebCore22EmptyFrameLoaderClient38dispatchDidLoadResourceFromMemoryCacheEPNS_14DocumentLoaderERKNS_15ResourceRequestERKNS_16ResourceResponseEi() == 0);
    Require(_ZN7WebCore23ISOSchemeInformationBox11boxTypeNameEv() == static_cast<int>(0x73636869u));
    Require(_ZN7WebCore26ISOProtectionSchemeInfoBox11boxTypeNameEv() == static_cast<int>(0x73696e66u));
    Require(_ZN7WebCore27screenSupportsExtendedColorEPNS_6WidgetE() == 0);
    Require(_ZN7WebCore36ISOProtectionSystemSpecificHeaderBox11boxTypeNameEv() == static_cast<int>(0x70737368u));
    Require(_ZN9Inspector18InjectedScriptHost19isHTMLAllCollectionERN3JSC2VMENS1_7JSValueE() == 0);
    Require(_ZN9Inspector18InjectedScriptHost21getInternalPropertiesERN3JSC2VMEPNS1_14JSGlobalObjectENS1_7JSValueE() == 0);
    Require(_ZN9Inspector18InjectedScriptHost7subtypeEPN3JSC14JSGlobalObjectENS1_7JSValueE() == static_cast<int>(0x0000000au));
    Require(_ZNK3JSC9CodeBlock26reoptimizationRetryCounterEv() == 0);
    Require(_ZNK7WebCore13MIMETypeCache11isAvailableEv() == 1);
    Require(_ZNK7WebCore14LoaderStrategy11usePingLoadEv() == 1);
    Require(_ZNK7WebCore14LoaderStrategy27havePerformedSecurityChecksERKNS_16ResourceResponseE() == 0);
    Require(_ZNK7WebCore14LoaderStrategy27shouldPerformSecurityChecksEv() == 0);
    Require(_ZNK7WebCore15ActiveDOMObject25virtualHasPendingActivityEv() == 0);
    Require(_ZNK7WebCore16TrackPrivateBase10trackIndexEv() == 0);
    Require(_ZNK7WebCore19BlurFilterOperation11movesPixelsEv() == 1);
    Require(_ZNK7WebCore19BlurFilterOperation14affectsOpacityEv() == 1);
    Require(_ZNK7WebCore20LowPowerModeNotifier21isLowPowerModeEnabledEv() == 0);
    Require(_ZNK7WebCore20ScrollingCoordinator25scrollableContainerNodeIDERKNS_12RenderObjectE() == 0);
    Require(_ZNK7WebCore22EmptyFrameLoaderClient10hasWebViewEv() == 1);
    Require(_ZNK7WebCore22EmptyFrameLoaderClient12canCachePageEv() == 0);
    Require(_ZNK7WebCore22EmptyFrameLoaderClient14shouldFallBackERKNS_13ResourceErrorE() == 0);
    Require(_ZNK7WebCore22EmptyFrameLoaderClient15canShowMIMETypeERKN3WTF6StringE() == 0);
    Require(_ZNK7WebCore22EmptyFrameLoaderClient16canHandleRequestERKNS_15ResourceRequestE() == 0);
    Require(_ZNK7WebCore22EmptyFrameLoaderClient21canShowMIMETypeAsHTMLERKN3WTF6StringE() == 0);
    Require(_ZNK7WebCore22EmptyFrameLoaderClient21shouldGoToHistoryItemERNS_11HistoryItemE() == 0);
    Require(_ZNK7WebCore22EmptyFrameLoaderClient24isEmptyFrameLoaderClientEv() == 1);
    Require(_ZNK7WebCore24CoordinatedGraphicsLayer26isCoordinatedGraphicsLayerEv() == 1);
    Require(_ZNK7WebCore25DropShadowFilterOperation11movesPixelsEv() == 1);
    Require(_ZNK7WebCore25DropShadowFilterOperation14affectsOpacityEv() == 1);
    Require(_ZNK7WebCore4Page17useDarkAppearanceEv() == 0);
    Require(_ZNK7WebCore6Quirks25needsYouTubeMouseOutQuirkEv() == 0);
    Require(_ZNK7WebCore6Quirks29shouldAvoidUsingIOS13ForGmailEv() == 0);
    Require(_ZNK7WebCore6Quirks32isNeverRichlyEditableForTouchBarEv() == 0);
    Require(_ZNK7WebCore6Quirks49isTouchBarUpdateSupressedForHiddenContentEditableEv() == 0);
    Require(_ZNK7WebCore6Quirks50shouldIgnoreAriaForFastPathContentObservationCheckEv() == 0);
    Require(_ZNK7WebCore8Document22touchEventHandlerCountEv() == 0);
    Require(_ZNK7bmalloc11IsoTLSEntry9alignmentEv() == static_cast<int>(0x00000008u));
    Require(_ZNK9Inspector15InspectorTarget13isProvisionalEv() == 0);
    Require(_ZNK9Inspector22RemoteAutomationTarget4typeEv() == 0);
    Require(_ZNK9Inspector22RemoteInspectionTarget26automaticInspectionAllowedEv() == 0);
    return 0;
}
