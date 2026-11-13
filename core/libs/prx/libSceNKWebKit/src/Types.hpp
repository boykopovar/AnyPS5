#pragma once

#include "prx/libSceNKWebKit/src/Object.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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
    static BundleScriptWorld* NormalWorld() {
        static auto world = Make<BundleScriptWorld>();
        return world.Get();
    }
};

class BundleFrame final : public ObjectOf<ObjectType::BundleFrame> {
public:
};

class BundleDOMWindowExtension final : public ObjectOf<ObjectType::BundleDOMWindowExtension> {
public:
    BundleDOMWindowExtension() = default;
    BundleDOMWindowExtension(Retained<BundleFrame> frame, Retained<BundleScriptWorld> scriptWorld)
        : frame(std::move(frame)), scriptWorld(std::move(scriptWorld)) {}

    Retained<BundleFrame> frame;
    Retained<BundleScriptWorld> scriptWorld;
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
    bool isSelected = false;
    bool isDownloadableMedia = false;
    bool mediaHasAudio = false;
    bool mediaIsInFullscreen = false;
    std::uint32_t mediaType = 0;
};

class BundleNodeHandle final : public ObjectOf<ObjectType::BundleNodeHandle> {
public:
    Retained<BundleNodeHandle> document;
    Retained<BundleFrame> documentFrame;
    Retained<BundleFrame> htmlIFrameElementContentFrame;
};

class Download final : public ObjectOf<ObjectType::Download> {
public:
    Retained<Object> request;
    Retained<Object> originatingPage;
    bool wasUserInitiated = false;
    bool isCancelled = false;
};

class AccessibilityObject final : public ObjectOf<ObjectType::AccessibilityObject> {
public:
    Retained<String> title;
    Retained<String> description;
    Retained<String> helpText;
    Retained<String> url;
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
            auto handler = std::move(decisionHandler);
            handler(true);
        }
    }

    void Deny() {
        state = State::Denied;
        if (decisionHandler) {
            auto handler = std::move(decisionHandler);
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
            auto handler = std::move(decisionHandler);
            handler(true);
        }
    }

    void Deny() {
        state = State::Denied;
        if (decisionHandler) {
            auto handler = std::move(decisionHandler);
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

class BundleInspector final : public ObjectOf<ObjectType::BundleInspector> {
public:
    bool isVisible = false;
    bool isProfilingPage = false;
    std::vector<std::string> evaluatedScriptsForTest;
};

class Connection final : public ObjectOf<ObjectType::Connection> {
public:
    Retained<Object> lastMessageName;
    Retained<Object> lastMessageBody;
    bool clientConfigured = false;
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

class Notification final : public ObjectOf<ObjectType::Notification> {
public:
    std::u16string title;
    std::u16string body;
    std::u16string iconURL;
    std::u16string tag;
    std::u16string lang;
    std::u16string dir;
    std::uint64_t notificationID = 0;
};

class Frame final : public ObjectOf<ObjectType::Frame> {
public:
    bool isDisplayingMarkupDocument = false;
    bool isDisplayingStandaloneImageDocument = false;
    bool isMainFrame = false;
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

}  // namespace NKWebKit
