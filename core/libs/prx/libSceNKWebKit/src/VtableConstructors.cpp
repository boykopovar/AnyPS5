#include "prx/libSceNKWebKit/include/VtableConstructorExports.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>

extern "C" {

void APS5_VABI NKWebKitVtableEmpty_nid_no_patch() {
}

std::uint64_t APS5_VABI NKWebKitVtableZero_nid_no_patch() {
    return 0;
}

void* APS5_VABI NKWebKitVtableNullResult_nid_no_patch(void* result) {
    std::memset(result, 0, 8);
    return result;
}

void* APS5_VABI NKWebKitVtableZeroResult_nid_no_patch(void* result) {
    std::memset(result, 0, 16);
    return result;
}

}

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_JSC_WeakHandleOwner_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_JSC_WeakHandleOwner_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN3JSC15WeakHandleOwnerD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN3JSC15WeakHandleOwnerD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableZero_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN3JSC15WeakHandleOwner8finalizeENS_6HandleINS_7UnknownEEEPv") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_JSC_TypedArrayController_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_JSC_TypedArrayController_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN3JSC20TypedArrayControllerD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN3JSC20TypedArrayControllerD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WTF_FunctionDispatcher_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WTF_FunctionDispatcher_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN3WTF18FunctionDispatcherD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN3WTF18FunctionDispatcherD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_MIMETypeCache_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_MIMETypeCache_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore13MIMETypeCacheD1Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore13MIMETypeCacheD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore13MIMETypeCache11isAvailableEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore13MIMETypeCache13canDecodeTypeERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore13MIMETypeCache14supportedTypesEv") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableZero_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore13MIMETypeCache26isUnsupportedContainerTypeERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore13MIMETypeCache21canDecodeExtendedTypeERKNS_11ContentTypeE") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_XPathNSResolver_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_XPathNSResolver_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore15XPathNSResolverD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore15XPathNSResolverD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_BlobRegistryImpl_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_BlobRegistryImpl_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore16BlobRegistryImplD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore16BlobRegistryImplD0Ev") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_VisitedLinkStore_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_VisitedLinkStore_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore16VisitedLinkStoreD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore16VisitedLinkStoreD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_FrameLoaderClient_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_FrameLoaderClient_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient11hasHTMLViewEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClientD1Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClientD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient31dispatchDidChangeProvisionalURLEv") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient29dispatchDidChangeMainDocumentEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient26dispatchWillChangeDocumentERKN3WTF3URLES4_") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient29dispatchDidNavigateWithinPageEv") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient22dispatchDidReceiveIconEv") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient23dispatchDidExplicitOpenERKN3WTF3URLERKNS1_6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient17dispatchDidLayoutEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient31dispatchDidReachLayoutMilestoneEN3WTF9OptionSetINS_15LayoutMilestoneEEE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient37dispatchDidReachVisuallyNonEmptyStateEv") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient15didDisownOpenerEv") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient29overrideContentSecurityPolicyEv") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient27registerForIconNotificationEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient29shouldAlwaysUsePluginDocumentERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient25shouldLoadMediaElementURLERKN3WTF3URLE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient21didChangeScrollOffsetEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient11allowScriptEb") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient38shouldForceUniversalAccessFromLocalURLERKN3WTF3URLE") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient22shouldPaintBrokenImageERKN3WTF3URLE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient29dispatchGlobalObjectAvailableERNS_15DOMWrapperWorldE") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient56dispatchWillDisconnectDOMWindowExtensionFromGlobalObjectEPNS_18DOMWindowExtensionE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient52dispatchDidReconnectDOMWindowExtensionToGlobalObjectEPNS_18DOMWindowExtensionE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient52dispatchWillDestroyGlobalObjectForDOMWindowExtensionEPNS_18DOMWindowExtensionE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient20willInjectUserScriptERNS_15DOMWrapperWorldE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient30completePageTransitionIfNeededEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient24isEmptyFrameLoaderClientEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient32isServiceWorkerFrameLoaderClientEv") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient24didRestoreScrollPositionEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient23getLoadDecisionForIconsERKN3WTF6VectorISt4pairIRNS_8LinkIconEmELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient15didCreateWindowERNS_9DOMWindowE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient29hasFrameSpecificStorageAccessEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient28didLoadFromRegistrableDomainEONS_17RegistrableDomainE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient24loadedSubresourceDomainsEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient47allowsContentJavaScriptFromMostRecentNavigationEv") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableZero_nid_no_patch") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_LibWebRTCProvider_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_LibWebRTCProvider_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17LibWebRTCProviderD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17LibWebRTCProviderD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17LibWebRTCProvider9setActiveEb") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableNullResult_nid_no_patch") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_PluginInfoProvider_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_PluginInfoProvider_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore18PluginInfoProviderD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore18PluginInfoProviderD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_UserContentProvider_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_UserContentProvider_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore19UserContentProviderD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore19UserContentProviderD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_EmptyFrameLoaderClient_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_EmptyFrameLoaderClient_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient11hasHTMLViewEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClientD1Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClientD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient10hasWebViewEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient18makeRepresentationEPNS_14DocumentLoaderE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient6pageIDEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient7frameIDEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient21forceLayoutForNonHTMLEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient17setCopiesOnScrollEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient19detachedFromParent2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient19detachedFromParent3Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableZero_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient38dispatchDidLoadResourceFromMemoryCacheEPNS_14DocumentLoaderERKNS_15ResourceRequestERKNS_16ResourceResponseEi") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient31dispatchDidDispatchOnloadEventsEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient50dispatchDidReceiveServerRedirectForProvisionalLoadEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient31dispatchDidChangeProvisionalURLEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient31dispatchDidCancelClientRedirectEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient33dispatchWillPerformClientRedirectERKN3WTF3URLEdNS1_8WallTimeENS_19LockBackForwardListE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient29dispatchDidChangeMainDocumentEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient26dispatchWillChangeDocumentERKN3WTF3URLES4_") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient29dispatchDidNavigateWithinPageEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient35dispatchDidChangeLocationWithinPageEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient30dispatchDidPushStateWithinPageEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient33dispatchDidReplaceStateWithinPageEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient29dispatchDidPopStateWithinPageEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient17dispatchWillCloseEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient22dispatchDidReceiveIconEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient31dispatchDidStartProvisionalLoadEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient23dispatchDidReceiveTitleERKNS_19StringWithDirectionE") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient30dispatchDidFailProvisionalLoadERKNS_13ResourceErrorENS_19WillContinueLoadingE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient19dispatchDidFailLoadERKNS_13ResourceErrorE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient29dispatchDidFinishDocumentLoadEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient21dispatchDidFinishLoadEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient23dispatchDidExplicitOpenERKN3WTF3URLERKNS1_6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient17dispatchDidLayoutEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient31dispatchDidReachLayoutMilestoneEN3WTF9OptionSetINS_15LayoutMilestoneEEE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient37dispatchDidReachVisuallyNonEmptyStateEv") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableZero_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient12dispatchShowEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient31dispatchDecidePolicyForResponseERKNS_16ResourceResponseERKNS_15ResourceRequestENS_21PolicyCheckIdentifierERKN3WTF6StringEONS8_8FunctionIFvNS_12PolicyActionES7_EEE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient38dispatchDecidePolicyForNewWindowActionERKNS_16NavigationActionERKNS_15ResourceRequestEPNS_9FormStateERKN3WTF6StringENS_21PolicyCheckIdentifierEONS9_8FunctionIFvNS_12PolicyActionESD_EEE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient39dispatchDecidePolicyForNavigationActionERKNS_16NavigationActionERKNS_15ResourceRequestERKNS_16ResourceResponseEPNS_9FormStateENS_18PolicyDecisionModeENS_21PolicyCheckIdentifierEON3WTF8FunctionIFvNS_12PolicyActionESD_EEE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient17cancelPolicyCheckEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient31dispatchUnableToImplementPolicyERKNS_13ResourceErrorE") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient22dispatchWillSubmitFormERNS_9FormStateEON3WTF17CompletionHandlerIFvvEEE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient24revertToProvisionalStateEPNS_14DocumentLoaderE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient20setMainDocumentErrorEPNS_14DocumentLoaderERKNS_13ResourceErrorE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient25setMainFrameDocumentReadyEb") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient13startDownloadERKNS_15ResourceRequestERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient15willChangeTitleEPNS_14DocumentLoaderE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient14didChangeTitleEPNS_14DocumentLoaderE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient27willReplaceMultipartContentEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient26didReplaceMultipartContentEv") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient15finishedLoadingEPNS_14DocumentLoaderE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient19updateGlobalHistoryEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient32updateGlobalHistoryRedirectLinksEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient21shouldGoToHistoryItemERNS_11HistoryItemE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient15didDisownOpenerEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient25didDisplayInsecureContentEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient21didRunInsecureContentERNS_14SecurityOriginERKN3WTF3URLE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient12didDetectXSSERKN3WTF3URLEb") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient14cancelledErrorERKNS_15ResourceRequestE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient12blockedErrorERKNS_15ResourceRequestE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient28blockedByContentBlockerErrorERKNS_15ResourceRequestE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient18cannotShowURLErrorERKNS_15ResourceRequestE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient31interruptedForPolicyChangeErrorERKNS_15ResourceRequestE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient23cannotShowMIMETypeErrorERKNS_16ResourceResponseE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient21fileDoesNotExistErrorERKNS_16ResourceResponseE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient25pluginWillHandleLoadErrorERKNS_16ResourceResponseE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient14shouldFallBackERKNS_13ResourceErrorE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient16canHandleRequestERKNS_15ResourceRequestE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient15canShowMIMETypeERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient21canShowMIMETypeAsHTMLERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient32representationExistsForURLSchemeERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient29generatedMIMETypeForURLSchemeERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient18frameLoadCompletedEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient19saveViewStateToItemERNS_11HistoryItemE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient16restoreViewStateEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient22provisionalLoadStartedEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient13didFinishLoadEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient31prepareForDataSourceReplacementEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient20createDocumentLoaderERKNS_15ResourceRequestERKNS_14SubstituteDataE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient26updateCachedDocumentLoaderERNS_14DocumentLoaderE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient8setTitleERKNS_19StringWithDirectionERKN3WTF3URLE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient9userAgentERKN3WTF3URLE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient29overrideContentSecurityPolicyEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient29savePlatformDataToCachedFrameEPNS_11CachedFrameE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient36transitionToCommittedFromCachedFrameEPNS_11CachedFrameE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient31transitionToCommittedForNewPageEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient30didRestoreFromBackForwardCacheEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient12canCachePageEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient33convertMainResourceLoadToDownloadEPNS_14DocumentLoaderERKNS_15ResourceRequestERKNS_16ResourceResponseE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient11createFrameERKN3WTF6StringERNS_21HTMLFrameOwnerElementE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient12createPluginERKNS_7IntSizeERNS_17HTMLPlugInElementERKN3WTF3URLERKNS6_6VectorINS6_6StringELm0ENS6_15CrashOnOverflowELm16ENS6_10FastMallocEEESG_RKSB_b") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient20redirectDataToPluginERNS_6WidgetE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient17objectContentTypeERKN3WTF3URLERKNS1_6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient17overrideMediaTypeEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient35dispatchDidClearWindowObjectInWorldERNS_15DOMWrapperWorldE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient27registerForIconNotificationEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient29shouldAlwaysUsePluginDocumentERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient25shouldLoadMediaElementURLERKN3WTF3URLE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient21didChangeScrollOffsetEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient11allowScriptEb") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient38shouldForceUniversalAccessFromLocalURLERKN3WTF3URLE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient23createNetworkingContextEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient22shouldPaintBrokenImageERKN3WTF3URLE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient29dispatchGlobalObjectAvailableERNS_15DOMWrapperWorldE") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient56dispatchWillDisconnectDOMWindowExtensionFromGlobalObjectEPNS_18DOMWindowExtensionE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient52dispatchDidReconnectDOMWindowExtensionToGlobalObjectEPNS_18DOMWindowExtensionE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient52dispatchWillDestroyGlobalObjectForDOMWindowExtensionEPNS_18DOMWindowExtensionE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient20willInjectUserScriptERNS_15DOMWrapperWorldE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient30completePageTransitionIfNeededEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore22EmptyFrameLoaderClient24isEmptyFrameLoaderClientEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient32isServiceWorkerFrameLoaderClientEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient11prefetchDNSERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient10sendH2PingERKN3WTF3URLEONS1_17CompletionHandlerIFvONSt12experimental15fundamentals_v38expectedINS1_7SecondsENS_13ResourceErrorEEEEEE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient24didRestoreScrollPositionEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient23getLoadDecisionForIconsERKN3WTF6VectorISt4pairIRNS_8LinkIconEmELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient15didCreateWindowERNS_9DOMWindowE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore22EmptyFrameLoaderClient29hasFrameSpecificStorageAccessEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore17FrameLoaderClient28didLoadFromRegistrableDomainEONS_17RegistrableDomainE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient24loadedSubresourceDomainsEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK7WebCore17FrameLoaderClient47allowsContentJavaScriptFromMostRecentNavigationEv") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableZero_nid_no_patch") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_StorageNamespaceProvider_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_StorageNamespaceProvider_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore24StorageNamespaceProviderD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore24StorageNamespaceProviderD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_ContentSecurityPolicyClient_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_ContentSecurityPolicyClient_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore27ContentSecurityPolicyClient26willSendCSPViolationReportERNS_7CSPInfoE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore27ContentSecurityPolicyClientD1Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore27ContentSecurityPolicyClientD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_InspectorFrontendClientLocal_Settings_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_WebCore_InspectorFrontendClientLocal_Settings_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore28InspectorFrontendClientLocal8SettingsD1Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore28InspectorFrontendClientLocal8SettingsD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore28InspectorFrontendClientLocal8Settings11getPropertyERKN3WTF6StringE") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore28InspectorFrontendClientLocal8Settings11setPropertyERKN3WTF6StringES5_") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN7WebCore28InspectorFrontendClientLocal8Settings14deletePropertyERKN3WTF6StringE") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_Inspector_RemoteInspector_Client_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_Inspector_RemoteInspector_Client_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN9Inspector15RemoteInspector6ClientD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN9Inspector15RemoteInspector6ClientD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK9Inspector15RemoteInspector6Client11browserNameEv") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZNK9Inspector15RemoteInspector6Client14browserVersionEv") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("NKWebKitVtableEmpty_nid_no_patch") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

__asm__(".data\n"
    ".globl " APS5_ASM_SYMBOL("NKWebKitVtable_Inspector_RemoteControllableTarget_nid_no_patch") "\n"
    ".balign 8\n" APS5_ASM_SYMBOL("NKWebKitVtable_Inspector_RemoteControllableTarget_nid_no_patch") ":\n"
    ".quad 0\n.quad 0\n"
    ".quad " APS5_ASM_SYMBOL("_ZN9Inspector24RemoteControllableTargetD2Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("_ZN9Inspector24RemoteControllableTargetD0Ev") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".quad " APS5_ASM_SYMBOL("__cxa_pure_virtual_nid_postfix") "\n"
    ".text\n");

extern "C" {

extern const void* const NKWebKitVtable_JSC_WeakHandleOwner_nid_no_patch[];
extern const void* const NKWebKitVtable_JSC_TypedArrayController_nid_no_patch[];
extern const void* const NKWebKitVtable_WTF_FunctionDispatcher_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_MIMETypeCache_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_XPathNSResolver_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_BlobRegistryImpl_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_VisitedLinkStore_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_FrameLoaderClient_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_LibWebRTCProvider_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_PluginInfoProvider_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_UserContentProvider_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_EmptyFrameLoaderClient_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_StorageNamespaceProvider_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_ContentSecurityPolicyClient_nid_no_patch[];
extern const void* const NKWebKitVtable_WebCore_InspectorFrontendClientLocal_Settings_nid_no_patch[];
extern const void* const NKWebKitVtable_Inspector_RemoteInspector_Client_nid_no_patch[];
extern const void* const NKWebKitVtable_Inspector_RemoteControllableTarget_nid_no_patch[];

}

namespace {

template<typename T> void Store(void* self, std::size_t offset, T value) {
    std::memcpy(static_cast<char*>(self) + offset, &value, sizeof(value));
}

void StoreVtable(void* self, const void* const* table) {
    Store<const void* const*>(self, 0, table + 2);
}

}  // namespace

extern "C" {

const void* const* APS5_VABI NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch() {
    return NKWebKitVtable_JSC_WeakHandleOwner_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch() {
    return NKWebKitVtable_JSC_TypedArrayController_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch() {
    return NKWebKitVtable_WTF_FunctionDispatcher_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_MIMETypeCache_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_XPathNSResolver_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_BlobRegistryImpl_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_VisitedLinkStore_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_FrameLoaderClient_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_LibWebRTCProvider_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_PluginInfoProvider_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_UserContentProvider_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_EmptyFrameLoaderClient_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_StorageNamespaceProvider_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_ContentSecurityPolicyClient_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch() {
    return NKWebKitVtable_WebCore_InspectorFrontendClientLocal_Settings_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch() {
    return NKWebKitVtable_Inspector_RemoteInspector_Client_nid_no_patch;
}

const void* const* APS5_VABI NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch() {
    return NKWebKitVtable_Inspector_RemoteControllableTarget_nid_no_patch;
}

void APS5_VABI _ZN3JSC15WeakHandleOwnerC1ERKS0_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN3JSC15WeakHandleOwnerC1ERKS0_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_JSC_WeakHandleOwner_nid_no_patch);
}

void APS5_VABI _ZN3JSC15WeakHandleOwnerC1Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN3JSC15WeakHandleOwnerC1Ev requires an object");
    StoreVtable(self, NKWebKitVtable_JSC_WeakHandleOwner_nid_no_patch);
}

void APS5_VABI _ZN3JSC15WeakHandleOwnerC2ERKS0_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN3JSC15WeakHandleOwnerC2ERKS0_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_JSC_WeakHandleOwner_nid_no_patch);
}

void APS5_VABI _ZN3JSC15WeakHandleOwnerC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN3JSC15WeakHandleOwnerC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_JSC_WeakHandleOwner_nid_no_patch);
}

void APS5_VABI _ZN3JSC20TypedArrayControllerC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN3JSC20TypedArrayControllerC2Ev requires an object");
    Store<std::uint32_t>(self, 0x8, 0x1);
    StoreVtable(self, NKWebKitVtable_JSC_TypedArrayController_nid_no_patch);
}

void APS5_VABI _ZN3WTF18FunctionDispatcherC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN3WTF18FunctionDispatcherC2Ev requires an object");
    Store<std::uint32_t>(self, 0x8, 0x1);
    StoreVtable(self, NKWebKitVtable_WTF_FunctionDispatcher_nid_no_patch);
}

void APS5_VABI _ZN7WebCore13MIMETypeCacheC1Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore13MIMETypeCacheC1Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_MIMETypeCache_nid_no_patch);
    Store<std::uint8_t>(self, 0x8, 0x0);
    Store<std::uint8_t>(self, 0x10, 0x0);
    Store<std::uint8_t>(self, 0x18, 0x0);
    Store<std::uint8_t>(self, 0x20, 0x0);
}

void APS5_VABI _ZN7WebCore13MIMETypeCacheC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore13MIMETypeCacheC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_MIMETypeCache_nid_no_patch);
    Store<std::uint8_t>(self, 0x8, 0x0);
    Store<std::uint8_t>(self, 0x10, 0x0);
    Store<std::uint8_t>(self, 0x18, 0x0);
    Store<std::uint8_t>(self, 0x20, 0x0);
}

void APS5_VABI _ZN7WebCore15XPathNSResolverC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore15XPathNSResolverC2Ev requires an object");
    Store<std::uint32_t>(self, 0x8, 0x1);
    StoreVtable(self, NKWebKitVtable_WebCore_XPathNSResolver_nid_no_patch);
}

void APS5_VABI _ZN7WebCore16BlobRegistryImplC1Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore16BlobRegistryImplC1Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_BlobRegistryImpl_nid_no_patch);
    std::memset(static_cast<char*>(self) + 0x8, 0, 16);
}

void APS5_VABI _ZN7WebCore16BlobRegistryImplC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore16BlobRegistryImplC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_BlobRegistryImpl_nid_no_patch);
    std::memset(static_cast<char*>(self) + 0x8, 0, 16);
}

void APS5_VABI _ZN7WebCore16VisitedLinkStoreC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore16VisitedLinkStoreC2Ev requires an object");
    Store<std::uint32_t>(self, 0x8, 0x1);
    StoreVtable(self, NKWebKitVtable_WebCore_VisitedLinkStore_nid_no_patch);
    Store<std::uint64_t>(self, 0x10, 0x0);
}

void APS5_VABI _ZN7WebCore17FrameLoaderClientC2ERKS0_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN7WebCore17FrameLoaderClientC2ERKS0_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_WebCore_FrameLoaderClient_nid_no_patch);
}

void APS5_VABI _ZN7WebCore17FrameLoaderClientC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore17FrameLoaderClientC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_FrameLoaderClient_nid_no_patch);
}

void APS5_VABI _ZN7WebCore17LibWebRTCProviderC1ERKS0_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN7WebCore17LibWebRTCProviderC1ERKS0_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_WebCore_LibWebRTCProvider_nid_no_patch);
}

void APS5_VABI _ZN7WebCore17LibWebRTCProviderC1Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore17LibWebRTCProviderC1Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_LibWebRTCProvider_nid_no_patch);
}

void APS5_VABI _ZN7WebCore17LibWebRTCProviderC2ERKS0_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN7WebCore17LibWebRTCProviderC2ERKS0_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_WebCore_LibWebRTCProvider_nid_no_patch);
}

void APS5_VABI _ZN7WebCore17LibWebRTCProviderC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore17LibWebRTCProviderC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_LibWebRTCProvider_nid_no_patch);
}

void APS5_VABI _ZN7WebCore18PluginInfoProviderC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore18PluginInfoProviderC2Ev requires an object");
    Store<std::uint32_t>(self, 0x8, 0x1);
    StoreVtable(self, NKWebKitVtable_WebCore_PluginInfoProvider_nid_no_patch);
    Store<std::uint64_t>(self, 0x10, 0x0);
}

void APS5_VABI _ZN7WebCore19UserContentProviderC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore19UserContentProviderC2Ev requires an object");
    Store<std::uint32_t>(self, 0x8, 0x1);
    StoreVtable(self, NKWebKitVtable_WebCore_UserContentProvider_nid_no_patch);
    std::memset(static_cast<char*>(self) + 0x10, 0, 16);
}

void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClientC1ERKS0_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN7WebCore22EmptyFrameLoaderClientC1ERKS0_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_WebCore_EmptyFrameLoaderClient_nid_no_patch);
}

void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClientC1Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore22EmptyFrameLoaderClientC1Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_EmptyFrameLoaderClient_nid_no_patch);
}

void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClientC2ERKS0_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN7WebCore22EmptyFrameLoaderClientC2ERKS0_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_WebCore_EmptyFrameLoaderClient_nid_no_patch);
}

void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClientC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore22EmptyFrameLoaderClientC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_EmptyFrameLoaderClient_nid_no_patch);
}

void APS5_VABI _ZN7WebCore24StorageNamespaceProviderC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore24StorageNamespaceProviderC2Ev requires an object");
    Store<std::uint32_t>(self, 0x8, 0x1);
    StoreVtable(self, NKWebKitVtable_WebCore_StorageNamespaceProvider_nid_no_patch);
    std::memset(static_cast<char*>(self) + 0x10, 0, 16);
}

void APS5_VABI _ZN7WebCore27ContentSecurityPolicyClientC2ERKS0_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN7WebCore27ContentSecurityPolicyClientC2ERKS0_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_WebCore_ContentSecurityPolicyClient_nid_no_patch);
}

void APS5_VABI _ZN7WebCore27ContentSecurityPolicyClientC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore27ContentSecurityPolicyClientC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_ContentSecurityPolicyClient_nid_no_patch);
}

void APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal8SettingsC1ERKS1_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN7WebCore28InspectorFrontendClientLocal8SettingsC1ERKS1_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_WebCore_InspectorFrontendClientLocal_Settings_nid_no_patch);
}

void APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal8SettingsC1Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore28InspectorFrontendClientLocal8SettingsC1Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_InspectorFrontendClientLocal_Settings_nid_no_patch);
}

void APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal8SettingsC2ERKS1_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN7WebCore28InspectorFrontendClientLocal8SettingsC2ERKS1_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_WebCore_InspectorFrontendClientLocal_Settings_nid_no_patch);
}

void APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal8SettingsC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN7WebCore28InspectorFrontendClientLocal8SettingsC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_WebCore_InspectorFrontendClientLocal_Settings_nid_no_patch);
}

void APS5_VABI _ZN9Inspector15RemoteInspector6ClientC2ERKS1_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN9Inspector15RemoteInspector6ClientC2ERKS1_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_Inspector_RemoteInspector_Client_nid_no_patch);
}

void APS5_VABI _ZN9Inspector15RemoteInspector6ClientC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN9Inspector15RemoteInspector6ClientC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_Inspector_RemoteInspector_Client_nid_no_patch);
}

void APS5_VABI _ZN9Inspector24RemoteControllableTargetC2ERKS0_(void* self, const void* other) {
    if (!self) throw std::invalid_argument("_ZN9Inspector24RemoteControllableTargetC2ERKS0_ requires an object");
    (void)other;
    StoreVtable(self, NKWebKitVtable_Inspector_RemoteControllableTarget_nid_no_patch);
    if (!other) throw std::invalid_argument("_ZN9Inspector24RemoteControllableTargetC2ERKS0_ requires a source object");
    std::memcpy(static_cast<char*>(self) + 0x8, static_cast<const char*>(other) + 0x8, 4);
}

void APS5_VABI _ZN9Inspector24RemoteControllableTargetC2Ev(void* self) {
    if (!self) throw std::invalid_argument("_ZN9Inspector24RemoteControllableTargetC2Ev requires an object");
    StoreVtable(self, NKWebKitVtable_Inspector_RemoteControllableTarget_nid_no_patch);
    Store<std::uint32_t>(self, 0x8, 0x0);
}

}
