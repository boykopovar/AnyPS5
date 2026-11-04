#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

extern "C" {
const void* APS5_VABI WKContextConfigurationCreate() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPoolConfiguration>());
}

const void* APS5_VABI WKContextConfigurationCreateWithLegacyOptions() {
    return WKContextConfigurationCreate();
}

bool APS5_VABI WKContextConfigurationAlwaysKeepAndReuseSwappedProcesses(const void* configuration) {
    return NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->alwaysKeepAndReuseSwappedProcesses;
}

void APS5_VABI WKContextConfigurationSetAlwaysKeepAndReuseSwappedProcesses(const void* configuration, bool keepAndReuse) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->alwaysKeepAndReuseSwappedProcesses = keepAndReuse;
}

const void* APS5_VABI WKContextConfigurationCopyApplicationCacheDirectory(const void* configuration) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return nullptr;
}

void APS5_VABI WKContextConfigurationSetApplicationCacheDirectory(const void* configuration, const void* applicationCacheDirectory) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    NKWebKit::CastOrNull<NKWebKit::String>(applicationCacheDirectory, __func__);
}

const void* APS5_VABI WKContextConfigurationCopyCustomClassesForParameterCoder(const void* configuration) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>());
}

void APS5_VABI WKContextConfigurationSetCustomClassesForParameterCoder(const void* configuration, const void* classesForCoder) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    NKWebKit::CastOrNull<NKWebKit::Array>(classesForCoder, __func__);
}

const void* APS5_VABI WKContextConfigurationCopyDiskCacheDirectory(const void* configuration) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return nullptr;
}

void APS5_VABI WKContextConfigurationSetDiskCacheDirectory(const void* configuration, const void* diskCacheDirectory) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    NKWebKit::CastOrNull<NKWebKit::String>(diskCacheDirectory, __func__);
}

std::int64_t APS5_VABI WKContextConfigurationDiskCacheSizeOverride(const void* configuration) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return 0;
}

void APS5_VABI WKContextConfigurationSetDiskCacheSizeOverride(const void* configuration, std::int64_t size) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    (void)size;
}

bool APS5_VABI WKContextConfigurationFullySynchronousModeIsAllowedForTesting(const void* configuration) {
    return NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->fullySynchronousModeIsAllowedForTesting;
}

void APS5_VABI WKContextConfigurationSetFullySynchronousModeIsAllowedForTesting(const void* configuration, bool allowed) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->fullySynchronousModeIsAllowedForTesting = allowed;
}

bool APS5_VABI WKContextConfigurationIgnoreSynchronousMessagingTimeoutsForTesting(const void* configuration) {
    return NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->ignoreSynchronousMessagingTimeoutsForTesting;
}

void APS5_VABI WKContextConfigurationSetIgnoreSynchronousMessagingTimeoutsForTesting(const void* configuration, bool ignore) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->ignoreSynchronousMessagingTimeoutsForTesting = ignore;
}

const void* APS5_VABI WKContextConfigurationCopyIndexedDBDatabaseDirectory(const void* configuration) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return nullptr;
}

void APS5_VABI WKContextConfigurationSetIndexedDBDatabaseDirectory(const void* configuration, const void* indexedDBDatabaseDirectory) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    NKWebKit::CastOrNull<NKWebKit::String>(indexedDBDatabaseDirectory, __func__);
}

const void* APS5_VABI WKContextConfigurationCopyInjectedBundlePath(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->injectedBundlePath);
}

void APS5_VABI WKContextConfigurationSetInjectedBundlePath(const void* configuration, const void* injectedBundlePath) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->injectedBundlePath =
        NKWebKit::OptionalString(injectedBundlePath, __func__);
}

const void* APS5_VABI WKContextConfigurationCopyLocalStorageDirectory(const void* configuration) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return nullptr;
}

void APS5_VABI WKContextConfigurationSetLocalStorageDirectory(const void* configuration, const void* localStorageDirectory) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    NKWebKit::CastOrNull<NKWebKit::String>(localStorageDirectory, __func__);
}

const void* APS5_VABI WKContextConfigurationCopyMediaKeysStorageDirectory(const void* configuration) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return nullptr;
}

void APS5_VABI WKContextConfigurationSetMediaKeysStorageDirectory(const void* configuration, const void* mediaKeysStorageDirectory) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    NKWebKit::CastOrNull<NKWebKit::String>(mediaKeysStorageDirectory, __func__);
}

const void* APS5_VABI WKContextConfigurationCopyNetworkProcessPath(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->networkProcessPath);
}

void APS5_VABI WKContextConfigurationSetNetworkProcessPath(const void* configuration, const void* networkProcessPath) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->networkProcessPath =
        NKWebKit::OptionalString(networkProcessPath, __func__);
}

const void* APS5_VABI WKContextConfigurationCopyOverrideLanguages(const void* configuration) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    auto array = NKWebKit::Make<NKWebKit::Array>();
    for (const auto& lang : pool->overrideLanguages) {
        array->elements.push_back(NKWebKit::Make<NKWebKit::String>(lang));
    }
    return NKWebKit::ToRef(array);
}

void APS5_VABI WKContextConfigurationSetOverrideLanguages(const void* configuration, const void* overrideLanguages) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    pool->overrideLanguages.clear();
    if (!overrideLanguages) return;
    const auto* array = NKWebKit::Cast<NKWebKit::Array>(overrideLanguages, __func__);
    for (const auto& element : array->elements) {
        if (!element) continue;
        if (element->Type() == NKWebKit::ObjectType::String) {
            pool->overrideLanguages.push_back(static_cast<const NKWebKit::String*>(element.Get())->characters);
        }
    }
}

bool APS5_VABI WKContextConfigurationPrewarmsProcessesAutomatically(const void* configuration) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return pool->isAutomaticProcessWarmingEnabledByClient.value_or(pool->clientWouldBenefitFromAutomaticProcessPrewarming);
}

void APS5_VABI WKContextConfigurationSetPrewarmsProcessesAutomatically(const void* configuration, bool prewarms) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->isAutomaticProcessWarmingEnabledByClient = prewarms;
}

bool APS5_VABI WKContextConfigurationProcessSwapsOnNavigation(const void* configuration) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return pool->processSwapsOnNavigationFromClient.value_or(pool->processSwapsOnNavigationFromExperimentalFeatures);
}

void APS5_VABI WKContextConfigurationSetProcessSwapsOnNavigation(const void* configuration, bool swaps) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->processSwapsOnNavigationFromClient = swaps;
}

const void* APS5_VABI WKContextConfigurationCopyResourceLoadStatisticsDirectory(const void* configuration) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return nullptr;
}

void APS5_VABI WKContextConfigurationSetResourceLoadStatisticsDirectory(const void* configuration, const void* resourceLoadStatisticsDirectory) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    NKWebKit::CastOrNull<NKWebKit::String>(resourceLoadStatisticsDirectory, __func__);
}

void APS5_VABI WKContextConfigurationSetShouldCaptureAudioInUIProcess(const void* configuration, bool allowed) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    (void)allowed;
}

void APS5_VABI WKContextConfigurationSetShouldConfigureJSCForTesting(const void* configuration, bool value) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->shouldConfigureJSCForTesting = value;
}

std::int32_t APS5_VABI WKContextConfigurationGetUserId(const void* configuration) {
    return NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->userId;
}

void APS5_VABI WKContextConfigurationSetUserId(const void* configuration, std::int32_t userId) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->userId = userId;
}

bool APS5_VABI WKContextConfigurationUsesWebProcessCache(const void* configuration) {
    return NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->usesWebProcessCache;
}

void APS5_VABI WKContextConfigurationSetUsesWebProcessCache(const void* configuration, bool uses) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->usesWebProcessCache = uses;
}

const void* APS5_VABI WKContextConfigurationCopyWebProcessPath(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->webProcessPath);
}

void APS5_VABI WKContextConfigurationSetWebProcessPath(const void* configuration, const void* webProcessPath) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__)->webProcessPath =
        NKWebKit::OptionalString(webProcessPath, __func__);
}

const void* APS5_VABI WKContextConfigurationCopyWebSQLDatabaseDirectory(const void* configuration) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    return nullptr;
}

void APS5_VABI WKContextConfigurationSetWebSQLDatabaseDirectory(const void* configuration, const void* webSQLDatabaseDirectory) {
    NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configuration, __func__);
    NKWebKit::CastOrNull<NKWebKit::String>(webSQLDatabaseDirectory, __func__);
}

}
