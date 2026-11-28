#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler;
using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisablerVtable;
using NKWebKit::WebCoreAbi::OpaqueOriginIdentifier;
using NKWebKit::WebCoreAbi::ResourceResponseStatusLayout;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WebCoreAbi::StoredCredentialsPolicy;
using NKWebKit::WtfAbi::ExpectedVoidString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "passesAccessControlCheck check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static const std::u16string wildcardMessage = u"Cannot use wildcard in Access-Control-Allow-Origin when credentials flag is true.";
static const std::u16string severalOriginsMessage = u"Access-Control-Allow-Origin cannot contain more than one origin.";
static const std::u16string credentialsMessage = u"Credentials flag is true, but Access-Control-Allow-Credentials is not \"true\".";

static std::u16string OriginMessage(std::u16string_view origin, std::u16string_view statusCode) {
    std::u16string message = u"Origin ";
    message += origin;
    message += u" is not allowed by Access-Control-Allow-Origin. Status code: ";
    message += statusCode;
    return message;
}

static String Text(std::string_view text, bool wide = false) {
    if (wide) {
        std::u16string units(text.begin(), text.end());
        return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
    }
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static std::u16string Contents(const StringImpl* impl) {
    if (!impl) return {};
    if (impl->hashAndFlags & 4) {
        const auto* bytes = static_cast<const unsigned char*>(impl->data);
        return {bytes, bytes + impl->length};
    }
    return {static_cast<const char16_t*>(impl->data), impl->length};
}

template<class Entry>
static void ReleaseHeaders(NKWebKit::HttpAbi::HeaderVector<Entry>& entries) {
    if (entries.buffer) {
        std::destroy_n(entries.buffer, entries.size);
        _ZN3WTF8fastFreeEPv(entries.buffer);
    }
}

struct Response {
    ResourceResponseStatusLayout layout{};

    Response() = default;
    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;
    ~Response() {
        ReleaseHeaders(layout.head.httpHeaderFields.common);
        ReleaseHeaders(layout.head.httpHeaderFields.uncommon);
    }

    void Set(HTTPHeaderName name, std::string_view value, bool wide = false) {
        _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&layout.head, name, Text(value, wide));
    }

    void AllowOrigin(std::string_view value, bool wide = false) {
        Set(HTTPHeaderName::AccessControlAllowOrigin, value, wide);
    }

    void AllowCredentials(std::string_view value, bool wide = false) {
        Set(HTTPHeaderName::AccessControlAllowCredentials, value, wide);
    }
};

struct Origin {
    SecurityOrigin origin{};
    bool tupleConstructed = false;

    Origin(std::string_view protocol, std::string_view host, std::optional<std::uint16_t> port = std::nullopt, bool wide = false) {
        new (&origin.data.storage.tuple) SecurityOriginTuple{Text(protocol, wide), Text(host, wide), {port.value_or(0), port.has_value()}};
        origin.data.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
        tupleConstructed = true;
    }
    explicit Origin(OpaqueOriginIdentifier identifier) {
        origin.data.storage.opaque = identifier;
        origin.data.index = NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;
    }
    Origin(const Origin&) = delete;
    Origin& operator=(const Origin&) = delete;
    ~Origin() {
        if (tupleConstructed) origin.data.storage.tuple.~SecurityOriginTuple();
    }

    void ForgetTuple(std::uint32_t index) {
        origin.data.storage.tuple.~SecurityOriginTuple();
        tupleConstructed = false;
        origin.data.storage.opaque = OpaqueOriginIdentifier{};
        origin.data.index = index;
    }
};

static int enabledCalls = 0;
static bool enabledAnswer = true;

static void APS5_VABI IgnoreDestructor(CrossOriginAccessControlCheckDisabler*) {
}

static bool APS5_VABI CountingEnabled(const CrossOriginAccessControlCheckDisabler*) {
    ++enabledCalls;
    return enabledAnswer;
}

static const CrossOriginAccessControlCheckDisablerVtable countingVtable{&IgnoreDestructor, &IgnoreDestructor, &CountingEnabled};

static CrossOriginAccessControlCheckDisabler Disabler(bool answer) {
    enabledCalls = 0;
    enabledAnswer = answer;
    return CrossOriginAccessControlCheckDisabler{&countingVtable, !answer};
}

static ExpectedVoidString Run(const Response& response, StoredCredentialsPolicy policy, const Origin& origin,
                              const CrossOriginAccessControlCheckDisabler* disabler = nullptr) {
    return _ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
        &response.layout.head, policy, &origin.origin, disabler);
}

static bool Passes(const ExpectedVoidString& result) {
    return result.hasValue;
}

static bool Fails(const ExpectedVoidString& result, std::u16string_view message) {
    return !result.hasValue && result.error && result.error->refCount == 2 && Contents(result.error) == message;
}

static bool Is8Bit(const ExpectedVoidString& result) {
    return result.error && (result.error->hashAndFlags & 4);
}

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static constexpr auto doNotUse = StoredCredentialsPolicy::DoNotUse;
static constexpr auto use = StoredCredentialsPolicy::Use;
static constexpr auto ephemeralStateless = StoredCredentialsPolicy::EphemeralStateless;

static void TestWildcard() {
    Origin origin("https", "example.com");
    Response response;
    response.AllowOrigin("*");
    Require(Passes(Run(response, doNotUse, origin)));
    Require(Fails(Run(response, use, origin), wildcardMessage));
    Require(Is8Bit(Run(response, use, origin)));
    Require(Fails(Run(response, ephemeralStateless, origin), wildcardMessage));
    Require(Fails(Run(response, static_cast<StoredCredentialsPolicy>(7), origin), wildcardMessage));

    response.AllowCredentials("true");
    Require(Fails(Run(response, use, origin), wildcardMessage));

    Response wide;
    wide.AllowOrigin("*", true);
    Require(Passes(Run(wide, doNotUse, origin)));
    Require(Fails(Run(wide, use, origin), wildcardMessage));

    for (const char* almost : {" *", "* ", "**", ""}) {
        Response other;
        other.AllowOrigin(almost);
        Require(Fails(Run(other, doNotUse, origin), OriginMessage(u"https://example.com", u"0")));
    }
}

static void TestWildcardSkipsTheOrigin() {
    Response response;
    response.AllowOrigin("*");
    Origin broken("https", "example.com");
    broken.ForgetTuple(7);
    Require(Passes(Run(response, doNotUse, broken)));
    bool rejected = false;
    try {
        Run(response, use, broken);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    Require(rejected);
}

static void TestDisabler() {
    Origin origin("https", "example.com");
    Response response;
    response.AllowOrigin("*");

    auto enabled = Disabler(true);
    Require(Fails(Run(response, use, origin, &enabled), wildcardMessage));
    Require(enabledCalls == 1);

    auto disabled = Disabler(false);
    Require(Passes(Run(response, use, origin, &disabled)));
    Require(enabledCalls == 1);

    auto ephemeral = Disabler(false);
    Require(Passes(Run(response, ephemeralStateless, origin, &ephemeral)));
    Require(enabledCalls == 1);

    auto unused = Disabler(true);
    Require(Passes(Run(response, doNotUse, origin, &unused)));
    Require(enabledCalls == 0);

    Response mismatch;
    mismatch.AllowOrigin("https://other.example");
    auto ignored = Disabler(false);
    Require(Fails(Run(mismatch, use, origin, &ignored), OriginMessage(u"https://example.com", u"0")));
    Require(enabledCalls == 1);

    Response match;
    match.AllowOrigin("https://example.com");
    auto credentials = Disabler(false);
    Require(Fails(Run(match, use, origin, &credentials), credentialsMessage));
    Require(enabledCalls == 1);

    auto& singleton = _ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv();
    Require(Fails(Run(response, use, origin, &singleton), wildcardMessage));
    _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(&singleton, false);
    Require(Passes(Run(response, use, origin, &singleton)));
    _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(&singleton, true);
    Require(Fails(Run(response, use, origin, &singleton), wildcardMessage));
}

static void TestOriginComparison() {
    Origin origin("https", "example.com");
    Response match;
    match.AllowOrigin("https://example.com");
    Require(Passes(Run(match, doNotUse, origin)));
    Require(Passes(Run(match, ephemeralStateless, origin)));

    Response wideHeader;
    wideHeader.AllowOrigin("https://example.com", true);
    Require(Passes(Run(wideHeader, doNotUse, origin)));

    Origin wideOrigin("https", "example.com", std::nullopt, true);
    Require(Passes(Run(match, doNotUse, wideOrigin)));

    Origin withPort("http", "example.com", std::uint16_t{8080});
    Response port;
    port.AllowOrigin("http://example.com:8080");
    Require(Passes(Run(port, doNotUse, withPort)));
    Require(Fails(Run(port, doNotUse, origin), OriginMessage(u"https://example.com", u"0")));
    Require(Fails(Run(match, doNotUse, withPort), OriginMessage(u"http://example.com:8080", u"0")));

    for (const char* other : {"HTTPS://EXAMPLE.COM", "https://example.com/", " https://example.com", "https://example.com ", "https://example.co",
                              "https://other.example", "null", ""}) {
        Response response;
        response.AllowOrigin(other);
        Require(Fails(Run(response, doNotUse, origin), OriginMessage(u"https://example.com", u"0")));
        Require(Fails(Run(response, use, origin), OriginMessage(u"https://example.com", u"0")));
    }

    Response absent;
    Require(Fails(Run(absent, doNotUse, origin), OriginMessage(u"https://example.com", u"0")));
    Require(Is8Bit(Run(absent, doNotUse, origin)));
    const auto wideMessage = Run(absent, doNotUse, wideOrigin);
    Require(Fails(wideMessage, OriginMessage(u"https://example.com", u"0")));
    Require(!Is8Bit(wideMessage));
}

static void TestSeveralOrigins() {
    Origin origin("https", "example.com");
    for (const char* list : {"https://a.example, https://example.com", "https://example.com,https://example.com", ",", "*,*", "https://example.com,"}) {
        Response response;
        response.AllowOrigin(list);
        Require(Fails(Run(response, doNotUse, origin), severalOriginsMessage));
        Require(Fails(Run(response, use, origin), severalOriginsMessage));
        Require(Is8Bit(Run(response, use, origin)));
    }
    Response wide;
    wide.AllowOrigin("https://a.example,https://b.example", true);
    Require(Fails(Run(wide, doNotUse, origin), severalOriginsMessage));
}

static void TestOpaqueAndFileOrigins() {
    Origin opaque(OpaqueOriginIdentifier{1, 2});
    Response null;
    null.AllowOrigin("null");
    Require(Passes(Run(null, doNotUse, opaque)));
    Response other;
    other.AllowOrigin("https://example.com");
    Require(Fails(Run(other, doNotUse, opaque), OriginMessage(u"null", u"0")));

    Origin file("file", "");
    Response fileHeader;
    fileHeader.AllowOrigin("file://");
    Require(Passes(Run(fileHeader, doNotUse, file)));
    Require(Fails(Run(null, doNotUse, file), OriginMessage(u"file://", u"0")));
    file.origin.enforcesFilePathSeparation = true;
    Require(Passes(Run(null, doNotUse, file)));
    Require(Fails(Run(fileHeader, doNotUse, file), OriginMessage(u"null", u"0")));
}

static void TestEmptyOrigin() {
    Origin empty("", "");
    Response absent;
    Require(Passes(Run(absent, doNotUse, empty)));
    Require(Fails(Run(absent, use, empty), credentialsMessage));
    Response present;
    present.AllowOrigin("https://example.com");
    const auto result = Run(present, doNotUse, empty);
    Require(Fails(result, OriginMessage(u"", u"0")));
    Require(Is8Bit(result));
    Response emptyHeader;
    emptyHeader.AllowOrigin("");
    Require(Fails(Run(emptyHeader, doNotUse, empty), OriginMessage(u"", u"0")));
}

static void TestCredentials() {
    Origin origin("https", "example.com");
    Response response;
    response.AllowOrigin("https://example.com");
    Require(Fails(Run(response, use, origin), credentialsMessage));
    Require(Is8Bit(Run(response, use, origin)));
    Require(Passes(Run(response, doNotUse, origin)));

    for (const char* value : {"True", "TRUE", "true ", " true", "1", "false", ""}) {
        response.AllowCredentials(value);
        Require(Fails(Run(response, use, origin), credentialsMessage));
        Require(Passes(Run(response, doNotUse, origin)));
        Require(Passes(Run(response, ephemeralStateless, origin)));
    }

    response.AllowCredentials("true");
    Require(Passes(Run(response, use, origin)));
    response.AllowCredentials("true", true);
    Require(Passes(Run(response, use, origin)));
}

static void TestStatusCode() {
    Origin origin("https", "example.com");
    Response response;
    response.AllowOrigin("https://other.example");
    std::memset(&response.layout.networkLoadMetrics, 0xFF, sizeof(response.layout.networkLoadMetrics));
    std::memset(&response.layout.certificateInfo, 0xFF, sizeof(response.layout.certificateInfo));
    auto* bytes = reinterpret_cast<unsigned char*>(&response.layout);
    for (std::size_t offset = 178; offset < sizeof(response.layout); ++offset) bytes[offset] = 0xFF;

    struct Case {
        short code;
        const char16_t* text;
    };
    for (const Case& item : {Case{0, u"0"}, Case{200, u"200"}, Case{404, u"404"}, Case{258, u"258"}, Case{32767, u"32767"}, Case{-1, u"-1"},
                             Case{-32768, u"-32768"}}) {
        std::memcpy(bytes + 176, &item.code, sizeof(item.code));
        Require(response.layout.httpStatusCode == item.code);
        Require(Fails(Run(response, doNotUse, origin), OriginMessage(u"https://example.com", item.text)));
        Require(Fails(Run(response, use, origin), OriginMessage(u"https://example.com", item.text)));
    }
}

static void TestReturnedObject() {
    Origin origin("https", "example.com");
    Response response;
    response.AllowOrigin("*");

    alignas(8) unsigned char storage[48];
    std::memset(storage, 0xA5, sizeof(storage));
    auto* passed = new (storage + 16) ExpectedVoidString(Run(response, doNotUse, origin));
    Require(storage[16 + 8] == 1);
    for (std::size_t index = 0; index < 16; ++index) Require(storage[index] == 0xA5);
    for (std::size_t index = 32; index < 48; ++index) Require(storage[index] == 0xA5);
    passed->~ExpectedVoidString();

    std::memset(storage, 0xA5, sizeof(storage));
    auto* failed = new (storage + 16) ExpectedVoidString(Run(response, use, origin));
    Require(storage[16 + 8] == 0);
    StringImpl* error = nullptr;
    std::memcpy(&error, storage + 16, sizeof(error));
    Require(error && error == failed->error);
    Require(Contents(error) == wildcardMessage);
    Require(error->refCount == 2);
    for (std::size_t index = 0; index < 16; ++index) Require(storage[index] == 0xA5);
    for (std::size_t index = 32; index < 48; ++index) Require(storage[index] == 0xA5);

    {
        ExpectedVoidString copy(*failed);
        Require(!copy.hasValue && copy.error == error && error->refCount == 4);
        ExpectedVoidString moved(std::move(copy));
        Require(!moved.hasValue && moved.error == error && copy.error == nullptr && error->refCount == 4);
    }
    Require(error->refCount == 2);
    failed->~ExpectedVoidString();
}

static void TestArgumentsAreLeftAlone() {
    Origin origin("https", "example.com");
    Response response;
    response.AllowOrigin("https://example.com");
    response.AllowCredentials("true");
    const auto& headers = response.layout.head.httpHeaderFields.common;
    Require(headers.size == 2);
    const unsigned originCount = headers.buffer[0].value.impl->refCount;
    const unsigned credentialsCount = headers.buffer[1].value.impl->refCount;
    const unsigned protocolCount = origin.origin.data.storage.tuple.protocol.impl->refCount;
    const unsigned references = origin.origin.refCount.load();
    Require(Passes(Run(response, use, origin)));
    Require(Fails(Run(response, use, Origin("https", "other.example")), OriginMessage(u"https://other.example", u"0")));
    Require(headers.size == 2);
    Require(headers.buffer[0].value.impl->refCount == originCount);
    Require(headers.buffer[1].value.impl->refCount == credentialsCount);
    Require(origin.origin.data.storage.tuple.protocol.impl->refCount == protocolCount);
    Require(origin.origin.refCount.load() == references);
}

static void TestInvalidArguments() {
    Origin origin("https", "example.com");
    Response response;
    response.AllowOrigin("*");
    Require(ThrowsInvalidArgument([&] {
        _ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            nullptr, doNotUse, &origin.origin, nullptr);
    }));
    Require(ThrowsInvalidArgument([&] {
        _ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            &response.layout.head, doNotUse, nullptr, nullptr);
    }));
    CrossOriginAccessControlCheckDisabler withoutTable{nullptr, false};
    Require(ThrowsInvalidArgument([&] { Run(response, use, origin, &withoutTable); }));
    Require(Passes(Run(response, doNotUse, origin, &withoutTable)));
}

int main() {
    TestWildcard();
    TestWildcardSkipsTheOrigin();
    TestDisabler();
    TestOriginComparison();
    TestSeveralOrigins();
    TestOpaqueAndFileOrigins();
    TestEmptyOrigin();
    TestCredentials();
    TestStatusCode();
    TestReturnedObject();
    TestArgumentsAreLeftAlone();
    TestInvalidArguments();
    std::puts("passesAccessControlCheck tests passed");
    return 0;
}
