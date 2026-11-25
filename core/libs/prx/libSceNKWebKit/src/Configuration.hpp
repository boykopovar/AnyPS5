#pragma once

#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Preferences.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NKWebKit {

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

    Retained<PageConfiguration> Duplicate() const {
        auto copy = Make<PageConfiguration>();
        copy->context = context;
        copy->pageGroup = pageGroup;
        copy->userContentController = userContentController;
        copy->preferences = preferences;
        copy->relatedPage = relatedPage;
        copy->websiteDataStore = websiteDataStore;
        copy->initialCapitalizationEnabled = initialCapitalizationEnabled;
        copy->cpuLimit = cpuLimit;
        return copy;
    }
};

class Page : public ObjectOf<ObjectType::Page> {
public:
    bool canGoBack = false;
    bool canGoForward = false;
    mutable Retained<Inspector> inspector;
    bool suppressScrollbarAnimations = false;
    bool hasHorizontalScrollbar = false;
    bool hasVerticalScrollbar = false;
    bool willHandleHorizontalScrollEvents = false;
    std::uint32_t scrollPinningBehavior = 0;
    bool useFixedLayout = false;
    SizeValue fixedLayoutSize{ 0.0, 0.0 };
    std::uint32_t paginationMode = 0;
    bool paginationBehavesLikeColumns = false;
    double gapBetweenPages = 0.0;
    double pageLength = 0.0;
    bool isPinnedToLeftSide = true;
    bool isPinnedToRightSide = true;
    bool isPinnedToTopSide = true;
    bool isPinnedToBottomSide = true;
    bool addsVisitedLinks = true;
    bool horizontalRubberBanding = true;
    bool verticalRubberBanding = true;
    bool rubberBandsAtBottom = true;
    bool rubberBandsAtLeft = true;
    bool rubberBandsAtRight = true;
    bool rubberBandsAtTop = true;
    double pageZoomFactor = 1.0;
    double textZoomFactor = 1.0;
    bool supportsTextZoom = true;
    bool isClosed = false;
    bool activeDOMObjectsAndAnimationsSuspended = false;
    std::optional<std::u16string> title;
    Retained<URL> activeURL;
    Retained<URL> provisionalURL;
    Retained<URL> committedURL;
    double estimatedProgress = 0.0;
    std::optional<std::u16string> applicationNameForUserAgent;
    std::optional<std::u16string> customUserAgent;
    std::optional<std::u16string> customTextEncodingName;
    bool backgroundExtendsBeyondPage = false;
    std::vector<std::string> loadedSubresourceDomains;
    bool userMediaStateCleared = false;
    bool wheelEventTestMonitorCleared = false;
    bool pointerLockAllowed = false;
    bool isTerminated = false;
    std::int32_t processIdentifier = 1000;
    std::vector<Retained<Object>> relatedPages;
    Retained<Object> websitePolicies;
    std::uint32_t layoutMilestones = 0;
    std::uint32_t mockCameraOrientation = 0;
    float mediaVolume = 1.0f;
    std::uint32_t mutedState = 0;
    bool mayStartMediaWhenInWindow = true;
    std::uint32_t mediaState = 0;
    bool allowsRemoteInspection = false;
    Retained<URL> pendingAPIRequestURL;
    Retained<PageConfiguration> configuration;
};
class WebsiteDataStore : public ObjectOf<ObjectType::WebsiteDataStore> {
public:
    std::vector<std::string> fetchCacheOrigins;
    std::unordered_map<std::string, std::uint64_t> fetchCacheSizes;
    bool removedAllFetchCaches = false;
};
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
    std::optional<std::u16string> cookieStoragePath;
    std::int32_t userId = -1;
    bool fullySynchronousModeIsAllowedForTesting = false;
    bool ignoreSynchronousMessagingTimeoutsForTesting = false;
    std::optional<bool> processSwapsOnNavigationFromClient;
    bool processSwapsOnNavigationFromExperimentalFeatures = false;
    bool processSwapsOnWindowOpenWithOpener = false;
    bool alwaysKeepAndReuseSwappedProcesses = false;
    std::optional<bool> isAutomaticProcessWarmingEnabledByClient;
    bool clientWouldBenefitFromAutomaticProcessPrewarming = false;
    bool usesWebProcessCache = false;
    bool shouldConfigureJSCForTesting = false;

    Retained<ProcessPoolConfiguration> Duplicate() const {
        auto copy = Make<ProcessPoolConfiguration>();
        copy->injectedBundlePath = injectedBundlePath;
        copy->webProcessPath = webProcessPath;
        copy->networkProcessPath = networkProcessPath;
        copy->cookieStoragePath = cookieStoragePath;
        copy->userId = userId;
        copy->fullySynchronousModeIsAllowedForTesting = fullySynchronousModeIsAllowedForTesting;
        copy->ignoreSynchronousMessagingTimeoutsForTesting = ignoreSynchronousMessagingTimeoutsForTesting;
        copy->processSwapsOnNavigationFromClient = processSwapsOnNavigationFromClient;
        copy->processSwapsOnNavigationFromExperimentalFeatures = processSwapsOnNavigationFromExperimentalFeatures;
        copy->processSwapsOnWindowOpenWithOpener = processSwapsOnWindowOpenWithOpener;
        copy->alwaysKeepAndReuseSwappedProcesses = alwaysKeepAndReuseSwappedProcesses;
        copy->isAutomaticProcessWarmingEnabledByClient = isAutomaticProcessWarmingEnabledByClient;
        copy->clientWouldBenefitFromAutomaticProcessPrewarming = clientWouldBenefitFromAutomaticProcessPrewarming;
        copy->usesWebProcessCache = usesWebProcessCache;
        copy->shouldConfigureJSCForTesting = shouldConfigureJSCForTesting;
        return copy;
    }
};

class ProcessPool final : public ObjectOf<ObjectType::ProcessPool> {
public:
    ProcessPool() : configuration(Make<ProcessPoolConfiguration>()) {}
    explicit ProcessPool(Retained<ProcessPoolConfiguration> config) : configuration(std::move(config)) {}

    Retained<ProcessPoolConfiguration> configuration;
    bool usesSingleWebProcess = false;
    bool accessibilityEnabled = false;
    bool processTerminationEnabled = true;
    std::vector<std::string> visitedLinks;
    bool alwaysUsesComplexTextCodePath = false;
    bool memoryCacheDisabled = false;
    bool httpPipeliningEnabled = true;
    bool shouldUseFontSmoothing = true;
    bool javaScriptConfigurationFileEnabled = false;
    bool javaScriptGarbageCollectorTimerEnabled = true;
    bool storageAccessAPIEnabled = false;
    std::uint32_t garbageCollectJavaScriptObjectsCount = 0;
    std::vector<std::string> bypassingCSPSchemes;
    std::vector<std::string> cachePartitionedSchemes;
    std::vector<std::string> canDisplayOnlyIfCanRequestSchemes;
    std::vector<std::string> emptyDocumentSchemes;
    std::vector<std::string> secureSchemes;
    std::vector<std::string> fontAllowList;
    bool initialProcessWarmed = false;
    bool useSeparateServiceWorkerProcess = false;
    bool currentModifierStateCleared = false;
std::uint32_t maximumNumberOfProcesses = 0xffffffffu;
    std::uint32_t processModel = 0;
    bool usesNetworkProcess = true;
    bool diskCacheSpeculativeValidationEnabled = false;
    bool canHandleHTTPSServerTrustEvaluation = true;
    Retained<String> customWebContentServiceBundleIdentifier;
    std::uint32_t terminatedNetworkProcessesCount = 0;
mutable Retained<IconDatabase> iconDatabase;
    std::optional<std::u16string> iconDatabasePath;
};

class WebsiteDataStoreConfiguration final : public ObjectOf<ObjectType::WebsiteDataStoreConfiguration> {
public:
    std::optional<std::u16string> applicationCacheDirectory = u"/app0/applications";
    std::optional<std::u16string> networkCacheDirectory = u"/app0/WebKitCache";
    std::optional<std::u16string> indexedDBDatabaseDirectory = u"/app0/indexeddb";
    std::optional<std::u16string> localStorageDirectory = u"/app0/local";
    std::optional<std::u16string> webSQLDatabaseDirectory = u"/app0/websql";
    std::optional<std::u16string> cacheStorageDirectory = u"/app0/CacheStorage";
    std::optional<std::u16string> mediaKeysStorageDirectory = u"/app0/mediakeys";
    std::optional<std::u16string> resourceLoadStatisticsDirectory = u"/app0/itp";
    std::optional<std::u16string> serviceWorkerRegistrationDirectory = u"/app0/serviceworkers";
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
    Retained<BundleScriptWorld> scriptWorld;
};

class UserStyleSheet final : public ObjectOf<ObjectType::UserStyleSheet> {
public:
    explicit UserStyleSheet(std::u16string source) : source(std::move(source)) {}

    std::u16string source;
    bool forMainFrameOnly = false;
    std::string baseURL = "about:blank";
    std::vector<std::u16string> allowedURLPatterns;
    std::vector<std::u16string> blockedURLPatterns;
    Retained<BundleScriptWorld> scriptWorld;
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
        : identifier(std::move(identifier)),
          preferences(Make<Preferences>(this->identifier)),
          userContentController(Make<UserContentController>()) {}

    std::u16string identifier;
    Retained<Object> preferences;
    Retained<UserContentController> userContentController;
    std::vector<Retained<Object>> userStyleSheets;
    std::u16string userStyleSheetLocation;
};

}  // namespace NKWebKit
