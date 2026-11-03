#pragma once

#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace NKWebKit {

enum class ObjectType : std::uint32_t {
    Null = 0,
    Array,
    AuthenticationChallenge,
    AuthenticationDecisionListener,
    CertificateInfo,
    Connection,
    ContextMenuItem,
    Credential,
    Data,
    Dictionary,
    Error,
    FrameHandle,
    Image,
    PageHandle,
    ProtectionSpace,
    RenderLayer,
    RenderObject,
    ResourceLoadInfo,
    SecurityOrigin,
    SessionState,
    SerializedScriptValue,
    String,
    URL,
    URLRequest,
    URLResponse,
    UserContentURLPattern,
    UserScript,
    UserStyleSheet,
    WebArchive,
    WebArchiveResource,
    Boolean,
    Double,
    UInt64,
    Int64,
    Point,
    Size,
    Rect,
    ApplicationCacheManager,
    Attachment,
    AutomationSession,
    BackForwardList,
    BackForwardListItem,
    CacheManager,
    ColorPickerResultListener,
    ContentRuleList,
    ContentRuleListAction,
    ContentRuleListStore,
    ContentWorld,
    ContextMenuListener,
    CustomHeaderFields,
    DataTask,
    DebuggableInfo,
    Download,
    Feature,
    FormSubmissionListener,
    Frame,
    FrameInfo,
    FramePolicyListener,
    FrameTreeNode,
    FullScreenManager,
    GeolocationManager,
    GeolocationPermissionRequest,
    HTTPCookieStore,
    HitTestResult,
    GeolocationPosition,
    GrammarDetail,
    IconDatabase,
    Inspector,
    InspectorConfiguration,
    KeyValueStorageManager,
    MediaCacheManager,
    MessageListener,
    Navigation,
    NavigationAction,
    NavigationData,
    NavigationResponse,
    Notification,
    NotificationManager,
    NotificationPermissionRequest,
    OpenPanelParameters,
    OpenPanelResultListener,
    OriginDataManager,
    Page,
    PageConfiguration,
    PageGroup,
    ProcessPool,
    ProcessPoolConfiguration,
    PluginSiteDataManager,
    Preferences,
    RequestStorageAccessConfirmResultListener,
    ResourceLoadStatisticsStore,
    ResourceLoadStatisticsFirstParty,
    ResourceLoadStatisticsThirdParty,
    RunBeforeUnloadConfirmPanelResultListener,
    RunJavaScriptAlertResultListener,
    RunJavaScriptConfirmResultListener,
    RunJavaScriptPromptResultListener,
    SpeechRecognitionPermissionCallback,
    TextChecker,
    URLSchemeTask,
    UserContentController,
    UserInitiatedAction,
    UserMediaPermissionCheck,
    UserMediaPermissionRequest,
    ViewportAttributes,
    VisitedLinkStore,
    WebResourceLoadStatisticsManager,
    WebsiteDataRecord,
    WebsiteDataStore,
    WebsiteDataStoreConfiguration,
    WebsitePolicies,
    WindowFeatures,
    MediaKeySystemPermissionCallback,
    QueryPermissionResultCallback,
    Bundle,
    BundleBackForwardList,
    BundleBackForwardListItem,
    BundleCSSStyleDeclarationHandle,
    BundleDOMWindowExtension,
    BundleFrame,
    BundleHitTestResult,
    BundleInspector,
    BundleNodeHandle,
    BundlePage,
    BundlePageBanner,
    BundlePageOverlay,
    BundleRangeHandle,
    BundleScriptWorld,
    EditCommandProxy,
    ObjCObjectGraph,
    View,
    PopupMenuItem,
    AccessibilityObject,
};

class Object {
public:
    Object() = default;
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    virtual ~Object() = default;
    virtual ObjectType Type() const = 0;
    void Ref() const { references.fetch_add(1, std::memory_order_relaxed); }
    void Deref() const {
        if (references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }

private:
    mutable std::atomic<std::uint32_t> references{1};
};

template<ObjectType kind>
class ObjectOf : public Object {
public:
    static constexpr ObjectType Kind = kind;
    ObjectType Type() const override { return kind; }
};

template<typename T>
class Retained {
public:
    Retained() = default;
    Retained(std::nullptr_t) {}
    static Retained Adopt(T* object) {
        Retained result;
        result.object = object;
        return result;
    }
    static Retained Share(T* object) {
        if (object) object->Ref();
        return Adopt(object);
    }
    Retained(const Retained& other) : object(other.object) {
        if (object) object->Ref();
    }
    Retained(Retained&& other) noexcept : object(std::exchange(other.object, nullptr)) {}
    template<typename U>
    Retained(Retained<U>&& other) noexcept : object(other.Leak()) {}
    Retained& operator=(Retained other) noexcept {
        std::swap(object, other.object);
        return *this;
    }
    ~Retained() {
        if (object) object->Deref();
    }
    T* Get() const { return object; }
    T* operator->() const { return object; }
    T& operator*() const { return *object; }
    explicit operator bool() const { return object != nullptr; }
    T* Leak() { return std::exchange(object, nullptr); }

private:
    T* object = nullptr;
};

template<typename T, typename... Arguments>
Retained<T> Make(Arguments&&... arguments) {
    return Retained<T>::Adopt(new T(std::forward<Arguments>(arguments)...));
}

inline const Object* ToObject(const void* ref, const char* function) {
    if (!ref) throw std::invalid_argument(std::string(function) + ": null object reference");
    return static_cast<const Object*>(ref);
}

template<typename T>
T* Cast(const void* ref, const char* function) {
    const auto* object = ToObject(ref, function);
    if (object->Type() != T::Kind) throw std::invalid_argument(std::string(function) + ": object reference of another type");
    return const_cast<T*>(static_cast<const T*>(object));
}

template<typename T>
T* CastOrNull(const void* ref, const char* function) {
    return ref ? Cast<T>(ref, function) : nullptr;
}

inline std::uint32_t TypeID(ObjectType type) {
    return static_cast<std::uint32_t>(type);
}

template<typename T>
const void* ToRef(Retained<T> object) {
    return static_cast<const Object*>(object.Leak());
}

}  // namespace NKWebKit
