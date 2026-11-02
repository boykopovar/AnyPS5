#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

extern "C" {
}

static void TestContextConfiguration() {
    const void* config = WKContextConfigurationCreate();
    Require(config != nullptr);
    Require(WKGetTypeID(config) == static_cast<std::uint32_t>(NKWebKit::ObjectType::ProcessPoolConfiguration));

    Require(!WKContextConfigurationAlwaysKeepAndReuseSwappedProcesses(config));
    WKContextConfigurationSetAlwaysKeepAndReuseSwappedProcesses(config, true);
    Require(WKContextConfigurationAlwaysKeepAndReuseSwappedProcesses(config));

    Require(!WKContextConfigurationFullySynchronousModeIsAllowedForTesting(config));
    WKContextConfigurationSetFullySynchronousModeIsAllowedForTesting(config, true);
    Require(WKContextConfigurationFullySynchronousModeIsAllowedForTesting(config));

    Require(!WKContextConfigurationIgnoreSynchronousMessagingTimeoutsForTesting(config));
    WKContextConfigurationSetIgnoreSynchronousMessagingTimeoutsForTesting(config, true);
    Require(WKContextConfigurationIgnoreSynchronousMessagingTimeoutsForTesting(config));

    Require(!WKContextConfigurationPrewarmsProcessesAutomatically(config));
    WKContextConfigurationSetPrewarmsProcessesAutomatically(config, true);
    Require(WKContextConfigurationPrewarmsProcessesAutomatically(config));

    Require(!WKContextConfigurationProcessSwapsOnNavigation(config));
    WKContextConfigurationSetProcessSwapsOnNavigation(config, true);
    Require(WKContextConfigurationProcessSwapsOnNavigation(config));

    Require(!WKContextConfigurationUsesWebProcessCache(config));
    WKContextConfigurationSetUsesWebProcessCache(config, true);
    Require(WKContextConfigurationUsesWebProcessCache(config));

    Require(WKContextConfigurationGetUserId(config) == -1);
    WKContextConfigurationSetUserId(config, 100);
    Require(WKContextConfigurationGetUserId(config) == 100);

    Require(WKContextConfigurationDiskCacheSizeOverride(config) == 0);
    WKContextConfigurationSetDiskCacheSizeOverride(config, 1024);

    Require(WKContextConfigurationCopyApplicationCacheDirectory(config) == nullptr);
    Require(WKContextConfigurationCopyDiskCacheDirectory(config) == nullptr);
    Require(WKContextConfigurationCopyIndexedDBDatabaseDirectory(config) == nullptr);
    Require(WKContextConfigurationCopyLocalStorageDirectory(config) == nullptr);
    Require(WKContextConfigurationCopyWebSQLDatabaseDirectory(config) == nullptr);
    Require(WKContextConfigurationCopyMediaKeysStorageDirectory(config) == nullptr);
    Require(WKContextConfigurationCopyResourceLoadStatisticsDirectory(config) == nullptr);

    WKContextConfigurationSetApplicationCacheDirectory(config, nullptr);
    WKContextConfigurationSetDiskCacheDirectory(config, nullptr);
    WKContextConfigurationSetIndexedDBDatabaseDirectory(config, nullptr);
    WKContextConfigurationSetLocalStorageDirectory(config, nullptr);
    WKContextConfigurationSetWebSQLDatabaseDirectory(config, nullptr);
    WKContextConfigurationSetMediaKeysStorageDirectory(config, nullptr);
    WKContextConfigurationSetResourceLoadStatisticsDirectory(config, nullptr);
    WKContextConfigurationSetShouldCaptureAudioInUIProcess(config, true);
    WKContextConfigurationSetShouldConfigureJSCForTesting(config, true);

    const void* classes = WKContextConfigurationCopyCustomClassesForParameterCoder(config);
    Require(classes != nullptr);
    Require(WKGetTypeID(classes) == static_cast<std::uint32_t>(NKWebKit::ObjectType::Array));
    Require(static_cast<const NKWebKit::Array*>(classes)->elements.empty());
    WKRelease(classes);
    WKContextConfigurationSetCustomClassesForParameterCoder(config, nullptr);

    const void* langs = WKContextConfigurationCopyOverrideLanguages(config);
    Require(langs != nullptr);
    Require(static_cast<const NKWebKit::Array*>(langs)->elements.empty());
    WKRelease(langs);

    auto langArray = NKWebKit::Make<NKWebKit::Array>();
    langArray->elements.push_back(NKWebKit::String::FromUtf8("en-US"));
    langArray->elements.push_back(NKWebKit::String::FromUtf8("ja-JP"));
    WKContextConfigurationSetOverrideLanguages(config, NKWebKit::ToRef(langArray));

    const void* copiedLangs = WKContextConfigurationCopyOverrideLanguages(config);
    Require(copiedLangs != nullptr);
    const auto* copiedArray = static_cast<const NKWebKit::Array*>(copiedLangs);
    Require(copiedArray->elements.size() == 2);
    Require(static_cast<const NKWebKit::String*>(copiedArray->elements[0].Get())->Utf8() == "en-US");
    Require(static_cast<const NKWebKit::String*>(copiedArray->elements[1].Get())->Utf8() == "ja-JP");
    WKRelease(copiedLangs);

    const void* bundleStr = NKWebKit::ToRef(NKWebKit::String::FromUtf8("/app0/bundle.prx"));
    WKContextConfigurationSetInjectedBundlePath(config, bundleStr);
    const void* copiedBundle = WKContextConfigurationCopyInjectedBundlePath(config);
    Require(copiedBundle != nullptr);
    Require(static_cast<const NKWebKit::String*>(copiedBundle)->Utf8() == "/app0/bundle.prx");
    WKRelease(bundleStr);
    WKRelease(copiedBundle);

    const void* webStr = NKWebKit::ToRef(NKWebKit::String::FromUtf8("/system/webprocess.elf"));
    WKContextConfigurationSetWebProcessPath(config, webStr);
    const void* copiedWeb = WKContextConfigurationCopyWebProcessPath(config);
    Require(copiedWeb != nullptr);
    Require(static_cast<const NKWebKit::String*>(copiedWeb)->Utf8() == "/system/webprocess.elf");
    WKRelease(webStr);
    WKRelease(copiedWeb);

    const void* netStr = NKWebKit::ToRef(NKWebKit::String::FromUtf8("/system/netprocess.elf"));
    WKContextConfigurationSetNetworkProcessPath(config, netStr);
    const void* copiedNet = WKContextConfigurationCopyNetworkProcessPath(config);
    Require(copiedNet != nullptr);
    Require(static_cast<const NKWebKit::String*>(copiedNet)->Utf8() == "/system/netprocess.elf");
    WKRelease(netStr);
    WKRelease(copiedNet);

    WKRelease(config);

    const void* legacyConfig = WKContextConfigurationCreateWithLegacyOptions();
    Require(legacyConfig != nullptr);
    Require(WKGetTypeID(legacyConfig) == static_cast<std::uint32_t>(NKWebKit::ObjectType::ProcessPoolConfiguration));
    WKRelease(legacyConfig);
}

static void TestPageConfiguration() {
    const void* config = WKPageConfigurationCreate();
    Require(config != nullptr);
    Require(WKGetTypeID(config) == static_cast<std::uint32_t>(NKWebKit::ObjectType::PageConfiguration));

    Require(WKPageConfigurationGetContext(config) == nullptr);
    Require(WKPageConfigurationGetPageGroup(config) == nullptr);
    Require(WKPageConfigurationGetUserContentController(config) == nullptr);
    Require(WKPageConfigurationGetPreferences(config) == nullptr);
    Require(WKPageConfigurationGetRelatedPage(config) == nullptr);
    Require(WKPageConfigurationGetWebsiteDataStore(config) == nullptr);

    const void* idStr = NKWebKit::ToRef(NKWebKit::String::FromUtf8("group1"));
    const void* pageGroup = WKPageGroupCreateWithIdentifier(idStr);
    WKRelease(idStr);
    WKPageConfigurationSetPageGroup(config, pageGroup);
    Require(WKPageConfigurationGetPageGroup(config) == pageGroup);

    const void* ucc = WKUserContentControllerCreate();
    WKPageConfigurationSetUserContentController(config, ucc);
    Require(WKPageConfigurationGetUserContentController(config) == ucc);

    WKPageConfigurationSetInitialCapitalizationEnabled(config, true);
    WKPageConfigurationSetBackgroundCPULimit(config, 0.75);

    WKRelease(ucc);
    WKRelease(pageGroup);
    WKRelease(config);
}

static void TestWebsiteDataStoreConfiguration() {
    const void* config = WKWebsiteDataStoreConfigurationCreate();
    Require(config != nullptr);
    Require(WKGetTypeID(config) == static_cast<std::uint32_t>(NKWebKit::ObjectType::WebsiteDataStoreConfiguration));

    Require(WKWebsiteDataStoreConfigurationGetPerOriginStorageQuota(config) == 1000ULL * 1024ULL * 1024ULL);
    WKWebsiteDataStoreConfigurationSetPerOriginStorageQuota(config, 2048);
    Require(WKWebsiteDataStoreConfigurationGetPerOriginStorageQuota(config) == 2048);

    Require(!WKWebsiteDataStoreConfigurationGetNetworkCacheSpeculativeValidationEnabled(config));
    WKWebsiteDataStoreConfigurationSetNetworkCacheSpeculativeValidationEnabled(config, true);
    Require(WKWebsiteDataStoreConfigurationGetNetworkCacheSpeculativeValidationEnabled(config));

    Require(!WKWebsiteDataStoreConfigurationGetTestingSessionEnabled(config));
    WKWebsiteDataStoreConfigurationSetTestingSessionEnabled(config, true);
    Require(WKWebsiteDataStoreConfigurationGetTestingSessionEnabled(config));

    Require(WKWebsiteDataStoreConfigurationGetStaleWhileRevalidateEnabled(config));
    WKWebsiteDataStoreConfigurationSetStaleWhileRevalidateEnabled(config, false);
    Require(!WKWebsiteDataStoreConfigurationGetStaleWhileRevalidateEnabled(config));

    const void* pathStr = NKWebKit::ToRef(NKWebKit::String::FromUtf8("/data/cache"));
    WKWebsiteDataStoreConfigurationSetApplicationCacheDirectory(config, pathStr);
    const void* c1 = WKWebsiteDataStoreConfigurationCopyApplicationCacheDirectory(config);
    Require(static_cast<const NKWebKit::String*>(c1)->Utf8() == "/data/cache");
    WKRelease(c1);

    WKWebsiteDataStoreConfigurationSetNetworkCacheDirectory(config, pathStr);
    const void* c2 = WKWebsiteDataStoreConfigurationCopyNetworkCacheDirectory(config);
    Require(static_cast<const NKWebKit::String*>(c2)->Utf8() == "/data/cache");
    WKRelease(c2);

    WKWebsiteDataStoreConfigurationSetIndexedDBDatabaseDirectory(config, pathStr);
    const void* c3 = WKWebsiteDataStoreConfigurationCopyIndexedDBDatabaseDirectory(config);
    Require(static_cast<const NKWebKit::String*>(c3)->Utf8() == "/data/cache");
    WKRelease(c3);

    WKWebsiteDataStoreConfigurationSetLocalStorageDirectory(config, pathStr);
    const void* c4 = WKWebsiteDataStoreConfigurationCopyLocalStorageDirectory(config);
    Require(static_cast<const NKWebKit::String*>(c4)->Utf8() == "/data/cache");
    WKRelease(c4);

    WKWebsiteDataStoreConfigurationSetWebSQLDatabaseDirectory(config, pathStr);
    const void* c5 = WKWebsiteDataStoreConfigurationCopyWebSQLDatabaseDirectory(config);
    Require(static_cast<const NKWebKit::String*>(c5)->Utf8() == "/data/cache");
    WKRelease(c5);

    WKWebsiteDataStoreConfigurationSetCacheStorageDirectory(config, pathStr);
    const void* c6 = WKWebsiteDataStoreConfigurationCopyCacheStorageDirectory(config);
    Require(static_cast<const NKWebKit::String*>(c6)->Utf8() == "/data/cache");
    WKRelease(c6);

    WKWebsiteDataStoreConfigurationSetMediaKeysStorageDirectory(config, pathStr);
    const void* c7 = WKWebsiteDataStoreConfigurationCopyMediaKeysStorageDirectory(config);
    Require(static_cast<const NKWebKit::String*>(c7)->Utf8() == "/data/cache");
    WKRelease(c7);

    WKWebsiteDataStoreConfigurationSetResourceLoadStatisticsDirectory(config, pathStr);
    const void* c8 = WKWebsiteDataStoreConfigurationCopyResourceLoadStatisticsDirectory(config);
    Require(static_cast<const NKWebKit::String*>(c8)->Utf8() == "/data/cache");
    WKRelease(c8);

    WKWebsiteDataStoreConfigurationSetServiceWorkerRegistrationDirectory(config, pathStr);
    const void* c9 = WKWebsiteDataStoreConfigurationCopyServiceWorkerRegistrationDirectory(config);
    Require(static_cast<const NKWebKit::String*>(c9)->Utf8() == "/data/cache");
    WKRelease(c9);

    WKWebsiteDataStoreConfigurationSetCookieStorageFile(config, pathStr);
    const void* c10 = WKWebsiteDataStoreConfigurationCopyCookieStorageFile(config);
    Require(static_cast<const NKWebKit::String*>(c10)->Utf8() == "/data/cache");
    WKRelease(c10);

    WKRelease(pathStr);
    WKRelease(config);
}

static void TestWebsitePolicies() {
    const void* policies = WKWebsitePoliciesCreate();
    Require(policies != nullptr);
    Require(WKGetTypeID(policies) == static_cast<std::uint32_t>(NKWebKit::ObjectType::WebsitePolicies));

    Require(WKWebsitePoliciesGetContentBlockersEnabled(policies));
    WKWebsitePoliciesSetContentBlockersEnabled(policies, false);
    Require(!WKWebsitePoliciesGetContentBlockersEnabled(policies));

    Require(WKWebsitePoliciesGetAllowedAutoplayQuirks(policies) == 0);
    WKWebsitePoliciesSetAllowedAutoplayQuirks(policies, 7);
    Require(WKWebsitePoliciesGetAllowedAutoplayQuirks(policies) == 7);

    Require(WKWebsitePoliciesGetAutoplayPolicy(policies) == 0);
    WKWebsitePoliciesSetAutoplayPolicy(policies, 2);
    Require(WKWebsitePoliciesGetAutoplayPolicy(policies) == 2);

    Require(WKWebsitePoliciesGetPopUpPolicy(policies) == 0);
    WKWebsitePoliciesSetPopUpPolicy(policies, 1);
    Require(WKWebsitePoliciesGetPopUpPolicy(policies) == 1);

    Require(WKWebsitePoliciesGetDataStore(policies) == nullptr);
    Require(WKWebsitePoliciesCopyCustomHeaderFields(policies) == nullptr);
    WKWebsitePoliciesSetCustomHeaderFields(policies, nullptr);

    WKRelease(policies);
}

static void TestUserScript() {
    const void* src = NKWebKit::ToRef(NKWebKit::String::FromUtf8("console.log('hi');"));
    const void* script = WKUserScriptCreateWithSource(src, 1, true);
    Require(script != nullptr);
    Require(WKGetTypeID(script) == static_cast<std::uint32_t>(NKWebKit::ObjectType::UserScript));

    const void* copiedSrc = WKUserScriptCopySource(script);
    Require(static_cast<const NKWebKit::String*>(copiedSrc)->Utf8() == "console.log('hi');");
    WKRelease(copiedSrc);

    Require(WKUserScriptGetInjectionTime(script) == 1);
    Require(WKUserScriptGetMainFrameOnly(script) == true);

    WKRelease(src);
    WKRelease(script);
}

static void TestUserContentController() {
    const void* ucc = WKUserContentControllerCreate();
    Require(ucc != nullptr);
    Require(WKGetTypeID(ucc) == static_cast<std::uint32_t>(NKWebKit::ObjectType::UserContentController));

    const void* scripts = WKUserContentControllerCopyUserScripts(ucc);
    Require(static_cast<const NKWebKit::Array*>(scripts)->elements.empty());
    WKRelease(scripts);

    const void* src = NKWebKit::ToRef(NKWebKit::String::FromUtf8("script1"));
    const void* s1 = WKUserScriptCreateWithSource(src, 0, false);
    WKRelease(src);
    WKUserContentControllerAddUserScript(ucc, s1);

    const void* scripts2 = WKUserContentControllerCopyUserScripts(ucc);
    Require(static_cast<const NKWebKit::Array*>(scripts2)->elements.size() == 1);
    WKRelease(scripts2);

    WKUserContentControllerRemoveAllUserScripts(ucc);
    const void* scripts3 = WKUserContentControllerCopyUserScripts(ucc);
    Require(static_cast<const NKWebKit::Array*>(scripts3)->elements.empty());
    WKRelease(scripts3);

    WKUserContentControllerRemoveAllUserContentFilters(ucc);

    WKRelease(s1);
    WKRelease(ucc);
}

static void TestCredential() {
    const void* u = NKWebKit::ToRef(NKWebKit::String::FromUtf8("user1"));
    const void* p = NKWebKit::ToRef(NKWebKit::String::FromUtf8("pass1"));
    const void* cred = WKCredentialCreate(u, p, 2);
    Require(cred != nullptr);
    Require(WKGetTypeID(cred) == static_cast<std::uint32_t>(NKWebKit::ObjectType::Credential));

    const void* copiedU = WKCredentialCopyUser(cred);
    Require(static_cast<const NKWebKit::String*>(copiedU)->Utf8() == "user1");
    WKRelease(copiedU);

    Require(WKCredentialCreateWithCertificateInfo(nullptr) == nullptr);

    WKRelease(u);
    WKRelease(p);
    WKRelease(cred);
}

static void TestGeolocationPosition() {
    const void* p1 = WKGeolocationPositionCreate(100.0, 35.68, 139.76, 5.0);
    Require(p1 != nullptr);
    Require(WKGetTypeID(p1) == static_cast<std::uint32_t>(NKWebKit::ObjectType::GeolocationPosition));

    const void* p2 = WKGeolocationPositionCreate_b(200.0, 35.68, 139.76, 5.0, true, 50.0, true, 2.0, true, 180.0, true, 10.0);
    Require(p2 != nullptr);
    Require(WKGetTypeID(p2) == static_cast<std::uint32_t>(NKWebKit::ObjectType::GeolocationPosition));

    const void* p3 = WKGeolocationPositionCreate_c(300.0, 35.68, 139.76, 5.0, true, 50.0, true, 2.0, true, 180.0, true, 10.0, true, 3.0);
    Require(p3 != nullptr);
    Require(WKGetTypeID(p3) == static_cast<std::uint32_t>(NKWebKit::ObjectType::GeolocationPosition));

    WKRelease(p1);
    WKRelease(p2);
    WKRelease(p3);
}

static void TestPageGroup() {
    const void* id = NKWebKit::ToRef(NKWebKit::String::FromUtf8("group_test"));
    const void* group = WKPageGroupCreateWithIdentifier(id);
    WKRelease(id);
    Require(group != nullptr);
    Require(WKGetTypeID(group) == static_cast<std::uint32_t>(NKWebKit::ObjectType::PageGroup));

    Require(WKPageGroupGetPreferences(group) == nullptr);

    const void* ucc = WKPageGroupGetUserContentController(group);
    Require(ucc != nullptr);
    Require(WKGetTypeID(ucc) == static_cast<std::uint32_t>(NKWebKit::ObjectType::UserContentController));

    const void* src = NKWebKit::ToRef(NKWebKit::String::FromUtf8("body { color: red; }"));
    WKPageGroupAddUserStyleSheet(group, src, nullptr, nullptr, nullptr, 0);
    WKPageGroupRemoveAllUserStyleSheets(group);

    const void* jssrc = NKWebKit::ToRef(NKWebKit::String::FromUtf8("var x = 1;"));
    WKPageGroupAddUserScript(group, jssrc, nullptr, nullptr, nullptr, 0, 0);
    WKPageGroupAddUserScript(group, jssrc, nullptr, nullptr, nullptr, 1, 0);
    const void* scripts = WKUserContentControllerCopyUserScripts(ucc);
    const auto* list = static_cast<const NKWebKit::Array*>(scripts);
    Require(list->elements.size() == 2);
    Require(!WKUserScriptGetMainFrameOnly(list->elements[0].Get()));
    Require(WKUserScriptGetMainFrameOnly(list->elements[1].Get()));
    WKRelease(scripts);

    WKPageGroupRemoveAllUserScripts(group);
    const void* scripts2 = WKUserContentControllerCopyUserScripts(ucc);
    Require(static_cast<const NKWebKit::Array*>(scripts2)->elements.empty());
    WKRelease(scripts2);

    WKRelease(src);
    WKRelease(jssrc);
    WKRelease(group);
}

static void TestStringCopiesFollowWebKit() {
    const void* config = WKContextConfigurationCreate();
    const void* unset = WKContextConfigurationCopyInjectedBundlePath(config);
    Require(unset != nullptr);
    Require(!static_cast<const NKWebKit::String*>(unset)->null);
    Require(static_cast<const NKWebKit::String*>(unset)->characters.empty());
    WKRelease(unset);
    bool threw = false;
    try {
        WKContextConfigurationSetInjectedBundlePath(config, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    WKRelease(config);
}

int main() {
    TestContextConfiguration();
    TestPageConfiguration();
    TestWebsiteDataStoreConfiguration();
    TestWebsitePolicies();
    TestUserScript();
    TestUserContentController();
    TestCredential();
    TestGeolocationPosition();
    TestPageGroup();
    TestStringCopiesFollowWebKit();
    std::puts("All config tests passed!");
    return 0;
}
