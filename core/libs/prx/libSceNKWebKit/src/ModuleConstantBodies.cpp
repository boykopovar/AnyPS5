#include "prx/libSceNKWebKit/include/ModuleConstantBodyExports.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void ClearBytes(void* target, std::size_t size, const char* function) {
    if (!target) throw std::invalid_argument(std::string(function) + " requires a result or object pointer");
    std::memset(target, 0, size);
}

}  // namespace

extern "C" {

void* APS5_VABI WKBundleNodeHandleGetHTMLInputElementAutoFillButtonBounds(void* result) {
    ClearBytes(result, 32, __func__);
    return result;
}

void* APS5_VABI WKBundleNodeHandleGetRenderRect(void* result) {
    ClearBytes(result, 32, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore10Pasteboard10readOriginEv(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore10Pasteboard20typesSafeForBindingsERKN3WTF6StringE(void* result) {
    ClearBytes(result, 16, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore10Pasteboard22readStringInCustomDataERKN3WTF6StringE(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore14SocketProvider22createWebSocketChannelERNS_8DocumentERNS_22WebSocketChannelClientE(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore17CredentialStorage29originsWithSessionCredentialsEv(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

bool APS5_VABI _ZN7WebCore17FrameLoaderClient29hasFrameSpecificStorageAccessEv() {
    return false;
}

bool APS5_VABI _ZN7WebCore17FrameLoaderClient38shouldForceUniversalAccessFromLocalURLERKN3WTF3URLE() {
    return false;
}

void* APS5_VABI _ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE(void* result) {
    ClearBytes(result, 16, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore21NetworkStorageSession17domCookiesForHostERKN3WTF6StringE(void* result) {
    ClearBytes(result, 16, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient11createFrameERKN3WTF6StringERNS_21HTMLFrameOwnerElementE(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient12createPluginERKNS_7IntSizeERNS_17HTMLPlugInElementERKN3WTF3URLERKNS6_6VectorINS6_6StringELm0ENS6_15CrashOnOverflowELm16ENS6_10FastMallocEEESG_RKSB_b(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void APS5_VABI _ZN7WebCore24CachedResourceHandleBaseC1Ev(void* self) {
    ClearBytes(self, 8, __func__);
}

void APS5_VABI _ZN7WebCore24CachedResourceHandleBaseC2Ev(void* self) {
    ClearBytes(self, 8, __func__);
}

void* APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal8Settings11getPropertyERKN3WTF6StringE(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore33signedPublicKeyAndChallengeStringEjRKN3WTF6StringERKNS0_3URLE(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void* APS5_VABI _ZN7WebCore4Icon18createIconForFilesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void APS5_VABI _ZN9Inspector27PerGlobalObjectWrapperWorldC1Ev(void* self) {
    ClearBytes(self, 8, __func__);
}

void APS5_VABI _ZN9Inspector27PerGlobalObjectWrapperWorldC2Ev(void* self) {
    ClearBytes(self, 8, __func__);
}

void* APS5_VABI _ZNK7WebCore14LoaderStrategy12ongoingLoadsEv(void* result) {
    ClearBytes(result, 16, __func__);
    return result;
}

void* APS5_VABI _ZNK7WebCore16ResourceResponse13cfURLResponseEv() {
    return nullptr;
}

bool APS5_VABI _ZNK7WebCore17FrameLoaderClient11hasHTMLViewEv() {
    return true;
}

bool APS5_VABI _ZNK7WebCore17FrameLoaderClient22shouldPaintBrokenImageERKN3WTF3URLE() {
    return true;
}

bool APS5_VABI _ZNK7WebCore17FrameLoaderClient24isEmptyFrameLoaderClientEv() {
    return false;
}

void* APS5_VABI _ZNK7WebCore17FrameLoaderClient24loadedSubresourceDomainsEv(void* result) {
    ClearBytes(result, 16, __func__);
    return result;
}

bool APS5_VABI _ZNK7WebCore17FrameLoaderClient25shouldLoadMediaElementURLERKN3WTF3URLE() {
    return true;
}

void* APS5_VABI _ZNK7WebCore17FrameLoaderClient29overrideContentSecurityPolicyEv(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

bool APS5_VABI _ZNK7WebCore17FrameLoaderClient29shouldAlwaysUsePluginDocumentERKN3WTF6StringE() {
    return false;
}

bool APS5_VABI _ZNK7WebCore17FrameLoaderClient32isServiceWorkerFrameLoaderClientEv() {
    return false;
}

bool APS5_VABI _ZNK7WebCore17FrameLoaderClient47allowsContentJavaScriptFromMostRecentNavigationEv() {
    return true;
}

void* APS5_VABI _ZNK7WebCore18PlatformPasteboard29typesSafeForDOMToReadAndWriteERKN3WTF6StringE(void* result) {
    ClearBytes(result, 16, __func__);
    return result;
}

void* APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient17overrideMediaTypeEv(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

bool APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient32representationExistsForURLSchemeERKN3WTF6StringE() {
    return false;
}

void* APS5_VABI _ZNK9Inspector15RemoteInspector6Client11browserNameEv(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void* APS5_VABI _ZNK9Inspector15RemoteInspector6Client14browserVersionEv(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void* APS5_VABI _ZNK9Inspector22RemoteInspectionTarget3urlEv(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

void* APS5_VABI _ZNK9Inspector22RemoteInspectionTarget4nameEv(void* result) {
    ClearBytes(result, 8, __func__);
    return result;
}

}
