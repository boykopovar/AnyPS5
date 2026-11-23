#pragma once

#include "prx/libSceNKWebKit/src/Object.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace NKWebKit {

std::optional<std::u16string> DecodeUtf8(const char* text, std::size_t length);
std::optional<std::u16string> DecodeUtf8(const char* text);
std::string EncodeUtf8(std::u16string_view text);
std::size_t ConvertUtf16ToUtf8(std::u16string_view text, char* buffer, std::size_t bufferSize, bool strict);

class String final : public ObjectOf<ObjectType::String> {
public:
    String() : null(true) {}
    explicit String(std::u16string text) : characters(std::move(text)) {}
    explicit String(std::optional<std::u16string> text) : characters(text.value_or(std::u16string())), null(!text.has_value()) {}
    static Retained<String> FromUtf8(const char* text) { return Make<String>(DecodeUtf8(text)); }
    std::string Utf8() const { return EncodeUtf8(characters); }

    std::u16string characters;
    bool null = false;
};

class URL final : public ObjectOf<ObjectType::URL> {
public:
    explicit URL(std::string text) : text(std::move(text)) {}

    std::string text;
};

class Array final : public ObjectOf<ObjectType::Array> {
public:
    Array() = default;
    explicit Array(std::vector<Retained<Object>> elements) : elements(std::move(elements)) {}

    std::vector<Retained<Object>> elements;
    bool mutableArray = false;
};

class Dictionary final : public ObjectOf<ObjectType::Dictionary> {
public:
    Dictionary() = default;
    explicit Dictionary(std::map<std::u16string, Retained<Object>> entries) : entries(std::move(entries)) {}

    std::map<std::u16string, Retained<Object>> entries;
    bool mutableDictionary = false;
};

class Data final : public ObjectOf<ObjectType::Data> {
public:
    explicit Data(std::vector<std::uint8_t> bytes) : bytes(std::move(bytes)) {}

    std::vector<std::uint8_t> bytes;
};

enum class SerializedScriptValueKind {
    Json,
    Undefined,
    NaN,
    PositiveInfinity,
    NegativeInfinity,
};

class SerializedScriptValue final : public ObjectOf<ObjectType::SerializedScriptValue> {
public:
    std::string data;
    SerializedScriptValueKind kind = SerializedScriptValueKind::Json;

    SerializedScriptValue() = default;
    SerializedScriptValue(std::string str, SerializedScriptValueKind k = SerializedScriptValueKind::Json)
        : data(std::move(str)), kind(k) {}
};

class SessionState final : public ObjectOf<ObjectType::SessionState> {
public:
    explicit SessionState(std::vector<std::uint8_t> data) : data(std::move(data)) {}

    std::vector<std::uint8_t> data;
};

class CertificateInfo final : public ObjectOf<ObjectType::CertificateInfo> {
};

template<ObjectType kind, typename Value>
class Number final : public ObjectOf<kind> {
public:
    explicit Number(Value value) : value(value) {}

    Value value;
};

using Boolean = Number<ObjectType::Boolean, bool>;
using Double = Number<ObjectType::Double, double>;
using UInt64 = Number<ObjectType::UInt64, std::uint64_t>;
using Int64 = Number<ObjectType::Int64, std::int64_t>;

struct PointValue {
    double x;
    double y;
};

struct SizeValue {
    double width;
    double height;
};

struct RectValue {
    PointValue origin;
    SizeValue size;
};

using Point = Number<ObjectType::Point, PointValue>;
using Size = Number<ObjectType::Size, SizeValue>;
using Rect = Number<ObjectType::Rect, RectValue>;

class UserContentURLPattern final : public ObjectOf<ObjectType::UserContentURLPattern> {
public:
    std::u16string scheme;
    std::u16string host;
    std::u16string path;
    bool matchSubdomains = false;
    bool valid = false;
};

class URLRequest final : public ObjectOf<ObjectType::URLRequest> {
public:
    URLRequest() = default;
    explicit URLRequest(Retained<URL> url) : url(std::move(url)) {}

    Retained<URL> url;
    Retained<URL> firstPartyForCookies;
    std::string httpMethod = "GET";
    Retained<Data> httpBody;
};

class URLResponse final : public ObjectOf<ObjectType::URLResponse> {
public:
    URLResponse() = default;
    URLResponse(Retained<URL> url, Retained<String> mimeType, int statusCode, Retained<String> suggestedFilename, bool isAttachment, std::int64_t expectedContentLength)
        : url(std::move(url)), mimeType(std::move(mimeType)), statusCode(statusCode), suggestedFilename(std::move(suggestedFilename)), isAttachment(isAttachment), expectedContentLength(expectedContentLength) {}

    Retained<URL> url;
    Retained<String> mimeType;
    int statusCode = 0;
    Retained<String> suggestedFilename;
    bool isAttachment = false;
    std::int64_t expectedContentLength = 0;
};

class HitTestResult final : public ObjectOf<ObjectType::HitTestResult> {
public:
    std::string absoluteImageURL;
    std::string absoluteLinkURL;
    std::string absoluteMediaURL;
    std::string absolutePDFURL;
    std::u16string linkLabel;
    std::u16string linkTitle;
    std::u16string lookupText;
    RectValue elementBoundingBox{{0.0, 0.0}, {0.0, 0.0}};
    bool isContentEditable = false;
};

class BundleScriptWorld final : public ObjectOf<ObjectType::BundleScriptWorld> {
public:
    Retained<String> name;
    std::uint64_t clearedWrappersCount = 0;
    bool allShadowRootsOpen = false;
    bool overrideBuiltinsDisabled = false;
    static BundleScriptWorld* NormalWorld() {
        static auto world = Make<BundleScriptWorld>();
        return world.Get();
    }
};

struct OpaqueJSContext;

class Bundle final : public ObjectOf<ObjectType::Bundle> {
public:
    ~Bundle() override;

    OpaqueJSContext* context = nullptr;
    std::size_t javaScriptObjectsCount = 0;
    std::uint32_t memoryReleaseCount = 0;
    bool historyClearedForTesting = false;
    bool resourceLoadStatisticsCleared = false;
    std::uint32_t resourceLoadStatisticsNotifyCount = 0;
    bool allowFileAccessFromFileURLs = false;
    bool allowStorageAccessFromFileURLs = false;
    bool allowUniversalAccessFromFileURLs = false;
    bool asynchronousSpellCheckingEnabled = false;
    bool authorAndUserStylesEnabled = true;
    std::uint64_t databaseQuota = 0;
    bool frameFlatteningEnabled = false;
    bool javaScriptCanAccessClipboard = false;
    int minimumLogicalFontSize = 0;
    bool popupBlockingEnabled = true;
    bool spatialNavigationEnabled = false;
};

class BundleFrame final : public ObjectOf<ObjectType::BundleFrame> {
public:
    RectValue contentBounds{{0.0, 0.0}, {0.0, 0.0}};
    RectValue visibleContentBounds{{0.0, 0.0}, {0.0, 0.0}};
    RectValue visibleContentBoundsExcludingScrollbars{{0.0, 0.0}, {0.0, 0.0}};
    SizeValue scrollOffset{0.0, 0.0};
    bool hasHorizontalScrollbar = false;
    bool hasVerticalScrollbar = false;
    int numberOfPages = 1;
    std::vector<bool> pageBoxVisibility;
    bool containsAnyFormElements = false;
    bool containsAnyFormControls = false;
    bool isFocused = false;
    bool hasOpener = false;
    bool allowsFollowingLink = true;
    bool shouldCloseOnWebView = true;
    std::optional<std::u16string> accessibleName;
    std::optional<std::u16string> textDirection;
    Retained<String> counterValue;
    Retained<String> innerText;
    Retained<String> layerTreeAsText;
    Retained<String> mimeTypeForResource;
    Retained<String> suggestedFilenameForResource;
    bool hasDocumentBackgroundColor = false;
    double documentBackgroundColor[4] = {0.0, 0.0, 0.0, 0.0};
};

class BundleDOMWindowExtension final : public ObjectOf<ObjectType::BundleDOMWindowExtension> {
public:
    BundleDOMWindowExtension() = default;
    BundleDOMWindowExtension(Retained<BundleFrame> frame, Retained<BundleScriptWorld> scriptWorld)
        : frame(std::move(frame)), scriptWorld(std::move(scriptWorld)) {}

    Retained<BundleFrame> frame;
    Retained<BundleScriptWorld> scriptWorld;
};

class BundleNodeHandle final : public ObjectOf<ObjectType::BundleNodeHandle> {
public:
    Retained<BundleNodeHandle> document;
    Retained<BundleFrame> documentFrame;
    Retained<BundleFrame> htmlIFrameElementContentFrame;
    Retained<String> htmlInputElementValue;
    bool htmlInputElementAutoFilled = false;
    bool htmlInputElementAutoFilledAndViewable = false;
    bool htmlInputElementSpellcheckEnabled = true;
    bool htmlInputElementLastChangeWasUserEdit = false;
    bool htmlTextAreaElementLastChangeWasUserEdit = false;
};

class BundleHitTestResult final : public ObjectOf<ObjectType::BundleHitTestResult> {
public:
    Retained<URL> absoluteImageURL;
    Retained<URL> absoluteLinkURL;
    Retained<URL> absoluteMediaURL;
    Retained<URL> absolutePDFURL;
    Retained<String> linkLabel;
    Retained<String> linkTitle;
    Retained<String> linkSuggestedFilename;
    Retained<BundleNodeHandle> nodeHandle;
    Retained<BundleNodeHandle> urlElementHandle;
    RectValue imageRect{{0.0, 0.0}, {0.0, 0.0}};
    bool isSelected = false;
    bool isDownloadableMedia = false;
    bool mediaHasAudio = false;
    bool mediaIsInFullscreen = false;
    std::uint32_t mediaType = 0;
};

class BundleInspector final : public ObjectOf<ObjectType::BundleInspector> {
public:
    bool isVisible = false;
    bool isProfilingPage = false;
    std::vector<std::string> evaluatedScriptsForTest;
};

class BundlePage final : public ObjectOf<ObjectType::BundlePage> {
public:
    mutable Retained<BundleInspector> inspector;
    double backingScaleFactor = 1.0;
    bool isControlledByAutomation = false;
    bool isSuspended = false;
    bool usesEphemeralSession = false;
    std::uint64_t renderTreeSize = 0;
    bool isFullScreen = false;
    bool hasComposition = false;
    Retained<Object> topOverhangImage;
    Retained<Object> bottomOverhangImage;
    std::uint64_t paintedObjectsCounterThreshold = 0;
};

class Download final : public ObjectOf<ObjectType::Download> {
public:
    Retained<Object> request;
    Retained<Object> originatingPage;
    std::uint64_t id = 0;
    Retained<Data> resumeData;
    Retained<Array> redirectChain;
    bool wasUserInitiated = true;
    bool isCancelled = false;
};

class AccessibilityObject final : public ObjectOf<ObjectType::AccessibilityObject> {
public:
    Retained<String> title;
    Retained<String> description;
    Retained<String> helpText;
    Retained<String> url;
    std::uint32_t role = 0;
    std::uint32_t buttonState = 0;
};

class MediaSessionMetadata final : public ObjectOf<ObjectType::MediaSessionMetadata> {
public:
    Retained<String> title;
    Retained<String> album;
    Retained<String> artist;
    Retained<URL> artworkURL;
};

class PopupMenuItem final : public ObjectOf<ObjectType::PopupMenuItem> {
public:
    Retained<String> text;
    bool enabled = true;
    bool isItem = true;
    bool isSeparator = false;
};

class OpenPanelResultListener final : public ObjectOf<ObjectType::OpenPanelResultListener> {
public:
    bool cancelled = false;
    std::vector<Retained<String>> chosenFiles;
};

class GeolocationPermissionRequest final : public ObjectOf<ObjectType::GeolocationPermissionRequest> {
public:
    enum class State { Pending, Allowed, Denied };
    using DecisionHandler = std::function<void(bool)>;

    GeolocationPermissionRequest() = default;
    explicit GeolocationPermissionRequest(DecisionHandler handler) : decisionHandler(std::move(handler)) {}

    void Allow() {
        state = State::Allowed;
        if (decisionHandler) {
            auto handler = std::exchange(decisionHandler, nullptr);
            handler(true);
        }
    }

    void Deny() {
        state = State::Denied;
        if (decisionHandler) {
            auto handler = std::exchange(decisionHandler, nullptr);
            handler(false);
        }
    }

    State state = State::Pending;
    DecisionHandler decisionHandler;
};

class NotificationPermissionRequest final : public ObjectOf<ObjectType::NotificationPermissionRequest> {
public:
    enum class State { Pending, Allowed, Denied };
    using DecisionHandler = std::function<void(bool)>;

    NotificationPermissionRequest() = default;
    explicit NotificationPermissionRequest(DecisionHandler handler) : decisionHandler(std::move(handler)) {}

    void Allow() {
        state = State::Allowed;
        if (decisionHandler) {
            auto handler = std::exchange(decisionHandler, nullptr);
            handler(true);
        }
    }

    void Deny() {
        state = State::Denied;
        if (decisionHandler) {
            auto handler = std::exchange(decisionHandler, nullptr);
            handler(false);
        }
    }

    State state = State::Pending;
    DecisionHandler decisionHandler;
};

class Inspector final : public ObjectOf<ObjectType::Inspector> {
public:
    bool isAttached = false;
    bool isConnected = false;
    bool isElementSelectionActive = false;
    bool isFront = false;
    bool isProfilingPage = false;
    bool isVisible = false;
};

class Connection final : public ObjectOf<ObjectType::Connection> {
public:
    Retained<Object> lastMessageName;
    Retained<Object> lastMessageBody;
    bool clientConfigured = false;
};

class MessageListener final : public ObjectOf<ObjectType::MessageListener> {
public:
    bool replied = false;
    Retained<Object> reply;

    void SendReply(Retained<Object> replyObj) {
        replied = true;
        reply = std::move(replyObj);
    }
};

class UserMediaPermissionRequest final : public ObjectOf<ObjectType::UserMediaPermissionRequest> {
public:
    bool decided = false;
    bool allowed = false;

    void Allow() {
        decided = true;
        allowed = true;
    }

    void Deny() {
        decided = true;
        allowed = false;
    }
};

enum class NotificationDirection : std::uint8_t { Auto, Ltr, Rtl };

std::uint64_t GenerateNotificationIdentifier();

class Notification final : public ObjectOf<ObjectType::Notification> {
public:
    std::u16string title;
    std::u16string body;
    std::u16string iconURL;
    std::u16string tag;
    std::u16string lang;
    NotificationDirection dir = NotificationDirection::Auto;
    std::uint64_t notificationID = GenerateNotificationIdentifier();
};

class Frame final : public ObjectOf<ObjectType::Frame> {
public:
    bool isDisplayingMarkupDocument = false;
    bool isDisplayingStandaloneImageDocument = false;
    bool isMainFrame = false;
    bool isFrameSet = false;
    std::uint64_t frameID = 0;
    std::uint32_t loadState = 2;
    Retained<URL> url;
    Retained<String> title;
    Retained<String> mimeType;
    Retained<URL> provisionalURL;
    Retained<URL> unreachableURL;
};

class Image final : public ObjectOf<ObjectType::Image> {
public:
    explicit Image(int width, int height) : width(width), height(height) {}

    int width = 0;
    int height = 0;
};

class NavigationAction final : public ObjectOf<ObjectType::NavigationAction> {
public:
    NavigationAction() = default;
    explicit NavigationAction(bool shouldPerformDownload) : shouldPerformDownload_(shouldPerformDownload) {}
    bool shouldPerformDownload() const noexcept { return shouldPerformDownload_; }
private:
    bool shouldPerformDownload_ = false;
};

class NavigationResponse final : public ObjectOf<ObjectType::NavigationResponse> {
public:
    NavigationResponse() = default;
    explicit NavigationResponse(bool canShowMIMEType) : canShowMIMEType_(canShowMIMEType) {}
    bool canShowMIMEType() const noexcept { return canShowMIMEType_; }
private:
    bool canShowMIMEType_ = true;
};

class AuthenticationDecisionListener final : public ObjectOf<ObjectType::AuthenticationDecisionListener> {
public:
    enum class Decision { None, Cancel, UseCredential, RejectProtectionSpace };
    Decision decision = Decision::None;
    Retained<Object> credential;
};

class AuthenticationChallenge final : public ObjectOf<ObjectType::AuthenticationChallenge> {
public:
    std::optional<std::string> requestURL;
    Retained<AuthenticationDecisionListener> decisionListener = Make<AuthenticationDecisionListener>();
    int previousFailureCount = 0;
    mutable Retained<Object> proposedCredential;
    mutable Retained<Object> protectionSpace;
    bool isForMainResource = false;
    bool isInMainFrame = false;
};

class SecurityOrigin final : public ObjectOf<ObjectType::SecurityOrigin> {
public:
    SecurityOrigin() = default;
    SecurityOrigin(const String& protocol, const String& host, std::optional<std::uint16_t> port)
        : protocol(protocol.characters), host(host.characters), port(port), protocolNull(protocol.null), hostNull(host.null) {}

    std::u16string protocol;
    std::u16string host;
    std::optional<std::uint16_t> port;
    bool protocolNull = false;
    bool hostNull = false;
};

class ResourceCacheManager final : public ObjectOf<ObjectType::CacheManager> {
public:
    std::uint32_t lastCachesCleared = 0;
    bool clearedAll = false;
    std::vector<std::string> origins;

    void ClearAll(std::uint32_t cachesToClear) {
        lastCachesCleared = cachesToClear;
        clearedAll = true;
        origins.clear();
    }

    void ClearForOrigin(const std::string& origin, std::uint32_t cachesToClear) {
        lastCachesCleared = cachesToClear;
        std::erase(origins, origin);
    }
};

class View final : public ObjectOf<ObjectType::View> {
public:
    double pageScaleFactor = 1.0;
    double viewScaleFactor = 1.0;
    bool drawsBackground = true;
    bool isFullScreen = false;
    bool isActive = false;
    bool isFocused = false;
    bool isVisible = false;
    SizeValue size{ 0.0, 0.0 };
};

class ContentRuleListStore final : public ObjectOf<ObjectType::ContentRuleListStore> {
public:
    std::u16string path;
    std::map<std::u16string, Retained<Object>> filters;
};

class FramePolicyListener final : public ObjectOf<ObjectType::FramePolicyListener> {
public:
    enum class Decision { None, Use, UseInNewProcess, Download, Ignore };

    Decision decision = Decision::None;
    Retained<Object> policies;
};

}  // namespace NKWebKit
