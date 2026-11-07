#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

extern "C" {
const void* APS5_VABI WKWebsiteDataStoreConfigurationCreate() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::WebsiteDataStoreConfiguration>());
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyApplicationCacheDirectory(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->applicationCacheDirectory);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetApplicationCacheDirectory(const void* configuration, const void* directory) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->applicationCacheDirectory =
        NKWebKit::OptionalString(directory, __func__);
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyNetworkCacheDirectory(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->networkCacheDirectory);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetNetworkCacheDirectory(const void* configuration, const void* directory) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->networkCacheDirectory =
        NKWebKit::OptionalString(directory, __func__);
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyIndexedDBDatabaseDirectory(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->indexedDBDatabaseDirectory);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetIndexedDBDatabaseDirectory(const void* configuration, const void* directory) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->indexedDBDatabaseDirectory =
        NKWebKit::OptionalString(directory, __func__);
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyLocalStorageDirectory(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->localStorageDirectory);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetLocalStorageDirectory(const void* configuration, const void* directory) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->localStorageDirectory =
        NKWebKit::OptionalString(directory, __func__);
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyWebSQLDatabaseDirectory(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->webSQLDatabaseDirectory);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetWebSQLDatabaseDirectory(const void* configuration, const void* directory) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->webSQLDatabaseDirectory =
        NKWebKit::OptionalString(directory, __func__);
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyCacheStorageDirectory(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->cacheStorageDirectory);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetCacheStorageDirectory(const void* configuration, const void* directory) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->cacheStorageDirectory =
        NKWebKit::OptionalString(directory, __func__);
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyMediaKeysStorageDirectory(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->mediaKeysStorageDirectory);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetMediaKeysStorageDirectory(const void* configuration, const void* directory) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->mediaKeysStorageDirectory =
        NKWebKit::OptionalString(directory, __func__);
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyResourceLoadStatisticsDirectory(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->resourceLoadStatisticsDirectory);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetResourceLoadStatisticsDirectory(const void* configuration, const void* directory) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->resourceLoadStatisticsDirectory =
        NKWebKit::OptionalString(directory, __func__);
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyServiceWorkerRegistrationDirectory(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->serviceWorkerRegistrationDirectory);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetServiceWorkerRegistrationDirectory(const void* configuration, const void* directory) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->serviceWorkerRegistrationDirectory =
        NKWebKit::OptionalString(directory, __func__);
}

const void* APS5_VABI WKWebsiteDataStoreConfigurationCopyCookieStorageFile(const void* configuration) {
    return NKWebKit::ToCopiedStringRef(NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->cookieStorageFile);
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetCookieStorageFile(const void* configuration, const void* cookieStorageFile) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->cookieStorageFile =
        NKWebKit::OptionalString(cookieStorageFile, __func__);
}

std::uint64_t APS5_VABI WKWebsiteDataStoreConfigurationGetPerOriginStorageQuota(const void* configuration) {
    return NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->perOriginStorageQuota;
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetPerOriginStorageQuota(const void* configuration, std::uint64_t quota) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->perOriginStorageQuota = quota;
}

bool APS5_VABI WKWebsiteDataStoreConfigurationGetNetworkCacheSpeculativeValidationEnabled(const void* configuration) {
    return NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->networkCacheSpeculativeValidationEnabled;
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetNetworkCacheSpeculativeValidationEnabled(const void* configuration, bool enabled) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->networkCacheSpeculativeValidationEnabled = enabled;
}

bool APS5_VABI WKWebsiteDataStoreConfigurationGetTestingSessionEnabled(const void* configuration) {
    return NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->testingSessionEnabled;
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetTestingSessionEnabled(const void* configuration, bool enabled) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->testingSessionEnabled = enabled;
}

bool APS5_VABI WKWebsiteDataStoreConfigurationGetStaleWhileRevalidateEnabled(const void* configuration) {
    return NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->staleWhileRevalidateEnabled;
}

void APS5_VABI WKWebsiteDataStoreConfigurationSetStaleWhileRevalidateEnabled(const void* configuration, bool enabled) {
    NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__)->staleWhileRevalidateEnabled = enabled;
}

}
