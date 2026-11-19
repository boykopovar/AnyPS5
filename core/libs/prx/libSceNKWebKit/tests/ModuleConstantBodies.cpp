#include "prx/libSceNKWebKit/include/ArgumentReturnExports.hpp"
#include "prx/libSceNKWebKit/include/ModuleConstantBodyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ModuleConstantBodies check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename F> static bool Throws(F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<typename F> static void CheckCleared(F f, std::size_t size) {
    alignas(16) unsigned char buffer[48];
    std::memset(buffer, 0xa5, sizeof(buffer));
    Require(f(buffer) == buffer);
    for (std::size_t i = 0; i < sizeof(buffer); ++i) Require(buffer[i] == (i < size ? 0 : 0xa5));
}

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    unsigned char storage = 0;
    CheckCleared(&WKBundleNodeHandleGetHTMLInputElementAutoFillButtonBounds, 32);
    Require(Throws([] { WKBundleNodeHandleGetHTMLInputElementAutoFillButtonBounds(nullptr); }));
    CheckCleared(&WKBundleNodeHandleGetRenderRect, 32);
    Require(Throws([] { WKBundleNodeHandleGetRenderRect(nullptr); }));
    CheckCleared(&_ZN7WebCore10Pasteboard10readOriginEv, 8);
    Require(Throws([] { _ZN7WebCore10Pasteboard10readOriginEv(nullptr); }));
    CheckCleared(&_ZN7WebCore10Pasteboard20typesSafeForBindingsERKN3WTF6StringE, 16);
    Require(Throws([] { _ZN7WebCore10Pasteboard20typesSafeForBindingsERKN3WTF6StringE(nullptr); }));
    CheckCleared(&_ZN7WebCore10Pasteboard22readStringInCustomDataERKN3WTF6StringE, 8);
    Require(Throws([] { _ZN7WebCore10Pasteboard22readStringInCustomDataERKN3WTF6StringE(nullptr); }));
    CheckCleared(&_ZN7WebCore14SocketProvider22createWebSocketChannelERNS_8DocumentERNS_22WebSocketChannelClientE, 8);
    Require(Throws([] { _ZN7WebCore14SocketProvider22createWebSocketChannelERNS_8DocumentERNS_22WebSocketChannelClientE(nullptr); }));
    CheckCleared(&_ZN7WebCore17CredentialStorage29originsWithSessionCredentialsEv, 8);
    Require(Throws([] { _ZN7WebCore17CredentialStorage29originsWithSessionCredentialsEv(nullptr); }));
    Require(!_ZN7WebCore17FrameLoaderClient29hasFrameSpecificStorageAccessEv());
    Require(!_ZN7WebCore17FrameLoaderClient38shouldForceUniversalAccessFromLocalURLERKN3WTF3URLE());
    CheckCleared(&_ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE, 16);
    Require(Throws([] { _ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE(nullptr); }));
    CheckCleared(&_ZN7WebCore21NetworkStorageSession17domCookiesForHostERKN3WTF6StringE, 16);
    Require(Throws([] { _ZN7WebCore21NetworkStorageSession17domCookiesForHostERKN3WTF6StringE(nullptr); }));
    CheckCleared(&_ZN7WebCore22EmptyFrameLoaderClient11createFrameERKN3WTF6StringERNS_21HTMLFrameOwnerElementE, 8);
    Require(Throws([] { _ZN7WebCore22EmptyFrameLoaderClient11createFrameERKN3WTF6StringERNS_21HTMLFrameOwnerElementE(nullptr); }));
    CheckCleared(&_ZN7WebCore22EmptyFrameLoaderClient12createPluginERKNS_7IntSizeERNS_17HTMLPlugInElementERKN3WTF3URLERKNS6_6VectorINS6_6StringELm0ENS6_15CrashOnOverflowELm16ENS6_10FastMallocEEESG_RKSB_b, 8);
    Require(Throws([] { _ZN7WebCore22EmptyFrameLoaderClient12createPluginERKNS_7IntSizeERNS_17HTMLPlugInElementERKN3WTF3URLERKNS6_6VectorINS6_6StringELm0ENS6_15CrashOnOverflowELm16ENS6_10FastMallocEEESG_RKSB_b(nullptr); }));
    CheckCleared([](void* p) { _ZN7WebCore24CachedResourceHandleBaseC1Ev(p); return p; }, 8);
    Require(Throws([] { _ZN7WebCore24CachedResourceHandleBaseC1Ev(nullptr); }));
    CheckCleared([](void* p) { _ZN7WebCore24CachedResourceHandleBaseC2Ev(p); return p; }, 8);
    Require(Throws([] { _ZN7WebCore24CachedResourceHandleBaseC2Ev(nullptr); }));
    CheckCleared(&_ZN7WebCore28InspectorFrontendClientLocal8Settings11getPropertyERKN3WTF6StringE, 8);
    Require(Throws([] { _ZN7WebCore28InspectorFrontendClientLocal8Settings11getPropertyERKN3WTF6StringE(nullptr); }));
    CheckCleared(&_ZN7WebCore33signedPublicKeyAndChallengeStringEjRKN3WTF6StringERKNS0_3URLE, 8);
    Require(Throws([] { _ZN7WebCore33signedPublicKeyAndChallengeStringEjRKN3WTF6StringERKNS0_3URLE(nullptr); }));
    CheckCleared(&_ZN7WebCore4Icon18createIconForFilesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE, 8);
    Require(Throws([] { _ZN7WebCore4Icon18createIconForFilesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(nullptr); }));
    CheckCleared([](void* p) { _ZN9Inspector27PerGlobalObjectWrapperWorldC1Ev(p); return p; }, 8);
    Require(Throws([] { _ZN9Inspector27PerGlobalObjectWrapperWorldC1Ev(nullptr); }));
    CheckCleared([](void* p) { _ZN9Inspector27PerGlobalObjectWrapperWorldC2Ev(p); return p; }, 8);
    Require(Throws([] { _ZN9Inspector27PerGlobalObjectWrapperWorldC2Ev(nullptr); }));
    CheckCleared(&_ZNK7WebCore14LoaderStrategy12ongoingLoadsEv, 16);
    Require(Throws([] { _ZNK7WebCore14LoaderStrategy12ongoingLoadsEv(nullptr); }));
    Require(_ZNK7WebCore16ResourceResponse13cfURLResponseEv() == nullptr);
    Require(_ZNK7WebCore17FrameLoaderClient11hasHTMLViewEv());
    Require(_ZNK7WebCore17FrameLoaderClient22shouldPaintBrokenImageERKN3WTF3URLE());
    Require(!_ZNK7WebCore17FrameLoaderClient24isEmptyFrameLoaderClientEv());
    CheckCleared(&_ZNK7WebCore17FrameLoaderClient24loadedSubresourceDomainsEv, 16);
    Require(Throws([] { _ZNK7WebCore17FrameLoaderClient24loadedSubresourceDomainsEv(nullptr); }));
    Require(_ZNK7WebCore17FrameLoaderClient25shouldLoadMediaElementURLERKN3WTF3URLE());
    CheckCleared(&_ZNK7WebCore17FrameLoaderClient29overrideContentSecurityPolicyEv, 8);
    Require(Throws([] { _ZNK7WebCore17FrameLoaderClient29overrideContentSecurityPolicyEv(nullptr); }));
    Require(!_ZNK7WebCore17FrameLoaderClient29shouldAlwaysUsePluginDocumentERKN3WTF6StringE());
    Require(!_ZNK7WebCore17FrameLoaderClient32isServiceWorkerFrameLoaderClientEv());
    Require(_ZNK7WebCore17FrameLoaderClient47allowsContentJavaScriptFromMostRecentNavigationEv());
    CheckCleared(&_ZNK7WebCore18PlatformPasteboard29typesSafeForDOMToReadAndWriteERKN3WTF6StringE, 16);
    Require(Throws([] { _ZNK7WebCore18PlatformPasteboard29typesSafeForDOMToReadAndWriteERKN3WTF6StringE(nullptr); }));
    CheckCleared(&_ZNK7WebCore22EmptyFrameLoaderClient17overrideMediaTypeEv, 8);
    Require(Throws([] { _ZNK7WebCore22EmptyFrameLoaderClient17overrideMediaTypeEv(nullptr); }));
    Require(!_ZNK7WebCore22EmptyFrameLoaderClient32representationExistsForURLSchemeERKN3WTF6StringE());
    CheckCleared(&_ZNK9Inspector15RemoteInspector6Client11browserNameEv, 8);
    Require(Throws([] { _ZNK9Inspector15RemoteInspector6Client11browserNameEv(nullptr); }));
    CheckCleared(&_ZNK9Inspector15RemoteInspector6Client14browserVersionEv, 8);
    Require(Throws([] { _ZNK9Inspector15RemoteInspector6Client14browserVersionEv(nullptr); }));
    CheckCleared(&_ZNK9Inspector22RemoteInspectionTarget3urlEv, 8);
    Require(Throws([] { _ZNK9Inspector22RemoteInspectionTarget3urlEv(nullptr); }));
    CheckCleared(&_ZNK9Inspector22RemoteInspectionTarget4nameEv, 8);
    Require(Throws([] { _ZNK9Inspector22RemoteInspectionTarget4nameEv(nullptr); }));
    Require(_ZN7WebCore17FrameLoaderClientnaEmPv(16, &storage) == &storage);
    Require(_ZN7WebCore17FrameLoaderClientnaEmPv(0, nullptr) == nullptr);
    Require(_ZN7WebCore17FrameLoaderClientnwEmPv(16, &storage) == &storage);
    Require(_ZN7WebCore17FrameLoaderClientnwEmPv(0, nullptr) == nullptr);
    Require(NotImplementedCalls_nid_no_patch() == before);
    return 0;
}
