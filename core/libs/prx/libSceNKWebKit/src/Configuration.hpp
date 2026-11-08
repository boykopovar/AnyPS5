#pragma once

#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace NKWebKit {

class ProcessPool : public ObjectOf<ObjectType::ProcessPool> {};
class Page : public ObjectOf<ObjectType::Page> {};
class Preferences : public ObjectOf<ObjectType::Preferences> {};
class WebsiteDataStore : public ObjectOf<ObjectType::WebsiteDataStore> {};
class ContentRuleList : public ObjectOf<ObjectType::ContentRuleList> {};

inline std::optional<std::u16string> OptionalString(const void* ref, const char* function) {
    const auto* str = Cast<String>(ref, function);
    if (str->null) return std::nullopt;
    return str->characters;
}

inline const void* ToCopiedStringRef(const std::optional<std::u16string>& value) {
    return ToRef(Make<String>(value.value_or(std::u16string())));
}

class ProcessPoolConfiguration final : public ObjectOf<ObjectType::ProcessPoolConfiguration> {
public:
    std::optional<std::u16string> injectedBundlePath;
    std::optional<std::u16string> webProcessPath;
    std::optional<std::u16string> networkProcessPath;
    std::int32_t userId = -1;
    bool fullySynchronousModeIsAllowedForTesting = false;
    bool ignoreSynchronousMessagingTimeoutsForTesting = false;
    std::optional<bool> processSwapsOnNavigationFromClient;
    bool processSwapsOnNavigationFromExperimentalFeatures = false;
    bool alwaysKeepAndReuseSwappedProcesses = false;
    std::optional<bool> isAutomaticProcessWarmingEnabledByClient;
    bool clientWouldBenefitFromAutomaticProcessPrewarming = false;
    bool usesWebProcessCache = false;
    bool shouldConfigureJSCForTesting = false;
    std::vector<std::u16string> overrideLanguages;
};

class PageConfiguration final : public ObjectOf<ObjectType::PageConfiguration> {
public:
    Retained<Object> context;
    Retained<Object> pageGroup;
    Retained<Object> userContentController;
    Retained<Object> preferences;
    Retained<Object> relatedPage;
    Retained<Object> websiteDataStore;
    bool initialCapitalizationEnabled = false;
    double cpuLimit = 0.0;
};

class WebsiteDataStoreConfiguration final : public ObjectOf<ObjectType::WebsiteDataStoreConfiguration> {
public:
    std::optional<std::u16string> applicationCacheDirectory;
    std::optional<std::u16string> networkCacheDirectory;
    std::optional<std::u16string> indexedDBDatabaseDirectory;
    std::optional<std::u16string> localStorageDirectory;
    std::optional<std::u16string> webSQLDatabaseDirectory;
    std::optional<std::u16string> cacheStorageDirectory;
    std::optional<std::u16string> mediaKeysStorageDirectory;
    std::optional<std::u16string> resourceLoadStatisticsDirectory;
    std::optional<std::u16string> serviceWorkerRegistrationDirectory;
    std::optional<std::u16string> cookieStorageFile;
    std::uint64_t perOriginStorageQuota = 1000ULL * 1024ULL * 1024ULL;
    bool networkCacheSpeculativeValidationEnabled = false;
    bool testingSessionEnabled = false;
    bool staleWhileRevalidateEnabled = true;
};

class WebsitePolicies final : public ObjectOf<ObjectType::WebsitePolicies> {
public:
    bool contentBlockersEnabled = true;
    std::uint32_t allowedAutoplayQuirks = 0;
    std::uint32_t autoplayPolicy = 0;
    std::uint32_t popUpPolicy = 0;
    Retained<Object> websiteDataStore;
};

class UserScript final : public ObjectOf<ObjectType::UserScript> {
public:
    UserScript(std::u16string source, std::uint32_t injectionTime, bool forMainFrameOnly)
        : source(std::move(source)), injectionTime(injectionTime), forMainFrameOnly(forMainFrameOnly) {}

    std::u16string source;
    std::uint32_t injectionTime = 0;
    bool forMainFrameOnly = false;
    std::string baseURL = "about:blank";
    std::vector<std::u16string> allowedURLPatterns;
    std::vector<std::u16string> blockedURLPatterns;
};

class UserStyleSheet final : public ObjectOf<ObjectType::UserStyleSheet> {
public:
    explicit UserStyleSheet(std::u16string source) : source(std::move(source)) {}

    std::u16string source;
    bool forMainFrameOnly = false;
    std::string baseURL = "about:blank";
    std::vector<std::u16string> allowedURLPatterns;
    std::vector<std::u16string> blockedURLPatterns;
};

class UserContentController final : public ObjectOf<ObjectType::UserContentController> {
public:
    std::vector<Retained<UserScript>> userScripts;
    std::vector<Retained<Object>> contentRuleLists;
};

class Credential final : public ObjectOf<ObjectType::Credential> {
public:
    Credential(std::u16string user, std::u16string password, std::uint32_t persistence)
        : user(std::move(user)), password(std::move(password)), persistence(persistence) {}

    std::u16string user;
    std::u16string password;
    std::uint32_t persistence = 0;
};

class GeolocationPosition final : public ObjectOf<ObjectType::GeolocationPosition> {
public:
    GeolocationPosition(double timestamp, double latitude, double longitude, double accuracy,
                        bool providesAltitude, double altitude,
                        bool providesAltitudeAccuracy, double altitudeAccuracy,
                        bool providesHeading, double heading,
                        bool providesSpeed, double speed,
                        bool providesFloorLevel, double floorLevel)
        : timestamp(timestamp), latitude(latitude), longitude(longitude), accuracy(accuracy),
          providesAltitude(providesAltitude), altitude(altitude),
          providesAltitudeAccuracy(providesAltitudeAccuracy), altitudeAccuracy(altitudeAccuracy),
          providesHeading(providesHeading), heading(heading),
          providesSpeed(providesSpeed), speed(speed),
          providesFloorLevel(providesFloorLevel), floorLevel(floorLevel) {}

    double timestamp = 0.0;
    double latitude = 0.0;
    double longitude = 0.0;
    double accuracy = 0.0;
    bool providesAltitude = false;
    double altitude = 0.0;
    bool providesAltitudeAccuracy = false;
    double altitudeAccuracy = 0.0;
    bool providesHeading = false;
    double heading = 0.0;
    bool providesSpeed = false;
    double speed = 0.0;
    bool providesFloorLevel = false;
    double floorLevel = 0.0;
};

class PageGroup final : public ObjectOf<ObjectType::PageGroup> {
public:
    explicit PageGroup(std::u16string identifier)
        : identifier(std::move(identifier)), userContentController(Make<UserContentController>()) {}

    std::u16string identifier;
    Retained<Object> preferences;
    Retained<UserContentController> userContentController;
    std::vector<Retained<Object>> userStyleSheets;
};

}  // namespace NKWebKit
