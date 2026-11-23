#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using NKWebKit::WebCoreAbi::ContentSecurityPolicy;
using NKWebKit::WebCoreAbi::ContentSecurityPolicyClient;
using NKWebKit::WebCoreAbi::ContentSecurityPolicyClientVtable;
using NKWebKit::WebCoreAbi::LegacyContentSecurityPolicyHeader;
using NKWebKit::WebCoreAbi::LegacyContentSecurityPolicyResponseHeaders;
using NKWebKit::WebCoreAbi::LegacyCSPInfo;
using NKWebKit::WebCoreAbi::LegacyFormData;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::LegacySecurityOrigin;
using NKWebKit::WebCoreAbi::LegacySecurityOriginVector;
using NKWebKit::WebCoreAbi::LegacySecurityOriginData;
using NKWebKit::WebCoreAbi::LegacySecurityPolicyViolationEventInit;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ContentSecurityPolicy check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TException, typename TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

static String Make(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static std::string Narrow(const StringImpl* impl) {
    std::string text;
    if (!impl) return text;
    const auto view = NKWebKit::WtfAbi::ViewOf(impl);
    for (unsigned i = 0; i < view.length; ++i) {
        const char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, i);
        text.push_back(unit > 0xFF ? '?' : static_cast<char>(unit));
    }
    return text;
}

static URL Parse(std::string_view text) {
    const String input = Make(text);
    const URL base{};
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, nullptr);
    return url;
}

struct RecordedViolation {
    std::string documentURI;
    std::string referrer;
    std::string blockedURI;
    std::string violatedDirective;
    std::string effectiveDirective;
    std::string originalPolicy;
    std::string sourceFile;
    bool sourceFileIsNull;
    unsigned statusCode;
    int lineNumber;
    int columnNumber;
    bool flags;
};

struct RecordedReport {
    std::string url;
    std::string body;
    unsigned refCount;
    unsigned bytesCapacity;
    unsigned elementsCapacity;
};

struct TestClient {
    ContentSecurityPolicyClient base;
    std::vector<std::string> messages;
    std::vector<unsigned> sources;
    std::vector<unsigned> levels;
    std::vector<std::uint64_t> requestIdentifiers;
    std::vector<std::string> announced;
    std::vector<RecordedViolation> violations;
    std::vector<RecordedReport> reports;
    std::vector<LegacyFormData*> taken;
    bool setSourceFile = false;
    bool takeReport = false;
};

static void APS5_VABI UnexpectedDestructor(ContentSecurityPolicyClient*) {
    std::abort();
}

static void APS5_VABI WillSendReport(ContentSecurityPolicyClient* self, LegacyCSPInfo* info) {
    auto* client = reinterpret_cast<TestClient*>(self);
    client->announced.push_back(Narrow(info->documentURI.impl));
    if (!client->setSourceFile) return;
    info->sourceFile = Make("https://example.com/script.js");
    info->lineNumber = 12;
    info->columnNumber = 34;
}

static void APS5_VABI EnqueueViolation(ContentSecurityPolicyClient* self, LegacySecurityPolicyViolationEventInit* init) {
    auto* client = reinterpret_cast<TestClient*>(self);
    client->violations.push_back(RecordedViolation{Narrow(init->documentURI.impl), Narrow(init->referrer.impl), Narrow(init->blockedURI.impl), Narrow(init->violatedDirective.impl), Narrow(init->effectiveDirective.impl), Narrow(init->originalPolicy.impl), Narrow(init->sourceFile.impl), init->sourceFile.impl == nullptr, init->statusCode, init->lineNumber, init->columnNumber, init->bubbles || init->cancelable || init->composed});
}

static void APS5_VABI SendReport(ContentSecurityPolicyClient* self, URL* url, LegacyFormData** report) {
    auto* client = reinterpret_cast<TestClient*>(self);
    LegacyFormData* data = *report;
    if (data->elementsSize != 1 || data->elements[0].index != 0 || data->identifier != 0 || data->alwaysStream || data->boundary || data->containsPasswordData || data->hasLengthInBytes) std::abort();
    client->reports.push_back(RecordedReport{Narrow(url->impl), std::string(data->elements[0].buffer, data->elements[0].size), data->refCount, data->elements[0].capacity, data->elementsCapacity});
    if (!client->takeReport) return;
    client->taken.push_back(data);
    *report = nullptr;
}

static void APS5_VABI AddConsoleMessage(ContentSecurityPolicyClient* self, std::uint8_t source, std::uint8_t level, const String* message, std::uint64_t requestIdentifier) {
    auto* client = reinterpret_cast<TestClient*>(self);
    client->messages.push_back(Narrow(message->impl));
    client->sources.push_back(source);
    client->levels.push_back(level);
    client->requestIdentifiers.push_back(requestIdentifier);
}

static const ContentSecurityPolicyClientVtable clientVtable = {WillSendReport, UnexpectedDestructor, UnexpectedDestructor, AddConsoleMessage, SendReport, EnqueueViolation};

struct Header {
    std::string text;
    bool enforce;
};

struct Policy {
    alignas(8) unsigned char storage[sizeof(ContentSecurityPolicy)];
    TestClient client;

    explicit Policy(std::string_view url = "https://example.com/", bool withClient = true) {
        client.base.vtable = &clientVtable;
        std::memset(storage, 0xCC, sizeof storage);
        URL protectedURL = Parse(url);
        _ZN7WebCore21ContentSecurityPolicyC1EON3WTF3URLEPNS_27ContentSecurityPolicyClientE(get(), &protectedURL, withClient ? &client.base : nullptr);
        Require(protectedURL.impl == nullptr);
    }
    Policy(const Policy&) = delete;
    ~Policy() { _ZN7WebCore21ContentSecurityPolicyD1Ev(get()); }

    ContentSecurityPolicy* get() { return reinterpret_cast<ContentSecurityPolicy*>(storage); }

    void Receive(std::initializer_list<Header> headers, bool reportParsingErrors = true, int httpStatusCode = 0, std::string_view referrer = {}) {
        LegacyContentSecurityPolicyResponseHeaders received;
        for (const Header& header : headers) received.headers.append(LegacyContentSecurityPolicyHeader{Make(header.text), header.enforce});
        received.httpStatusCode = httpStatusCode;
        String given = referrer.empty() ? String() : Make(referrer);
        _ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE(get(), &received, &given, reportParsingErrors);
        Require(given.impl == nullptr);
    }
};

static std::vector<std::string> MessagesFor(std::string_view header, bool enforce = true) {
    Policy policy;
    policy.Receive({{std::string(header), enforce}});
    for (std::size_t i = 0; i < policy.client.messages.size(); ++i) {
        Require(policy.client.sources[i] == 8);
        Require(policy.client.levels[i] == 2);
        Require(policy.client.requestIdentifiers[i] == 0);
    }
    return policy.client.messages;
}

static bool Only(const std::vector<std::string>& messages, std::string_view expected) {
    return messages.size() == 1 && messages[0] == expected;
}

static void TestConstruction() {
    {
        URL protectedURL = Parse("https://example.com:8443/a/b?q");
        StringImpl* urlImpl = protectedURL.impl;
        urlImpl->refCount += 2;
        const unsigned held = urlImpl->refCount;
        alignas(8) unsigned char storage[sizeof(ContentSecurityPolicy)];
        std::memset(storage, 0xCC, sizeof storage);
        auto* policy = reinterpret_cast<ContentSecurityPolicy*>(storage);
        TestClient client;
        client.base.vtable = &clientVtable;
        _ZN7WebCore21ContentSecurityPolicyC2EON3WTF3URLEPNS_27ContentSecurityPolicyClientE(policy, &protectedURL, &client.base);
        Require(protectedURL.impl == nullptr);
        Require(policy->scriptExecutionContext == nullptr);
        Require(policy->client == &client.base);
        Require(policy->protectedURL.impl == urlImpl);
        Require(urlImpl->refCount == held);
        Require(policy->selfSource != nullptr);
        Require(Narrow(policy->selfSourceProtocol.impl) == "https");
        Require(policy->policies.size == 0 && policy->policies.buffer == nullptr);
        Require(policy->lastPolicyEvalDisabledErrorMessage.impl == nullptr);
        Require(policy->lastPolicyWebAssemblyDisabledErrorMessage.impl == nullptr);
        Require(policy->referrer.impl == nullptr);
        Require(policy->sandboxFlags == 0);
        Require(!policy->overrideInlineStyleAllowed && policy->isReportingEnabled && !policy->upgradeInsecureRequests && !policy->hasAPIPolicy);
        Require(policy->httpStatusCode == 0);
        Require(policy->hashAlgorithmsForInlineScripts == 0 && policy->hashAlgorithmsForInlineStylesheets == 0);
        Require(policy->insecureNavigationRequestsToUpgrade == nullptr);
        Require(!policy->hasCachedResponseHeaders);
        _ZN7WebCore21ContentSecurityPolicyD2Ev(policy);
        Require(urlImpl->refCount == held - 2);
        NKWebKit::WtfAbi::Release(urlImpl);
        Require(client.messages.empty());
    }
    {
        Policy opaque("data:text/html,x");
        Require(opaque.get()->selfSourceProtocol.impl != nullptr);
        Require(opaque.get()->selfSourceProtocol.impl->length == 0);
    }
    {
        Policy plain("http://plain.example/");
        Require(Narrow(plain.get()->selfSourceProtocol.impl) == "http");
    }
    alignas(8) unsigned char storage[sizeof(ContentSecurityPolicy)];
    URL url{};
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore21ContentSecurityPolicyC1EON3WTF3URLEPNS_27ContentSecurityPolicyClientE(nullptr, &url, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore21ContentSecurityPolicyC1EON3WTF3URLEPNS_27ContentSecurityPolicyClientE(reinterpret_cast<ContentSecurityPolicy*>(storage), nullptr, nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore21ContentSecurityPolicyD1Ev(nullptr); }));
}

static void TestHeaders() {
    Policy policy;
    String referrer = Make("https://referrer.example/page");
    StringImpl* referrerImpl = referrer.impl;
    referrerImpl->refCount += 2;
    const unsigned held = referrerImpl->refCount;
    {
        LegacyContentSecurityPolicyResponseHeaders received;
        received.headers.append(LegacyContentSecurityPolicyHeader{Make("default-src 'self'; script-src https://a.example 'nonce-abc', frame-ancestors 'none'"), true});
        received.headers.append(LegacyContentSecurityPolicyHeader{Make("img-src *"), false});
        received.httpStatusCode = 404;
        _ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE(policy.get(), &received, &referrer, true);
    }
    Require(referrer.impl == nullptr);
    Require(policy.get()->referrer.impl == referrerImpl && referrerImpl->refCount == held);
    Require(policy.get()->httpStatusCode == 404);
    Require(policy.get()->policies.size == 3);
    Require(policy.get()->isReportingEnabled);
    Require(Only(policy.client.messages, "The Content Security Policy 'img-src *' was delivered in report-only mode, but does not specify a 'report-uri'; the policy will have no effect. Please either add a 'report-uri' directive, or deliver the policy via the 'Content-Security-Policy' header."));
    Require(_ZNK7WebCore21ContentSecurityPolicy22overridesXFrameOptionsEv(policy.get()));
    Require(!policy.get()->hasCachedResponseHeaders);
    {
        const LegacyContentSecurityPolicyResponseHeaders headers = _ZNK7WebCore21ContentSecurityPolicy15responseHeadersEv(policy.get());
        Require(policy.get()->hasCachedResponseHeaders);
        Require(headers.headers.size == 3 && headers.headers.capacity == 3);
        Require(Narrow(headers.headers[0].text.impl) == "default-src 'self'; script-src https://a.example 'nonce-abc'" && headers.headers[0].enforce);
        Require(Narrow(headers.headers[1].text.impl) == " frame-ancestors 'none'" && headers.headers[1].enforce);
        Require(Narrow(headers.headers[2].text.impl) == "img-src *" && !headers.headers[2].enforce);
        Require(headers.httpStatusCode == 404 && !headers.emptyForMarkable);
    }
    policy.Receive({}, true, 500, "https://other.example/");
    Require(policy.get()->httpStatusCode == 500);
    Require(Narrow(policy.get()->referrer.impl) == "https://other.example/");
    Require(referrerImpl->refCount == held - 2);
    NKWebKit::WtfAbi::Release(referrerImpl);
    {
        const LegacyContentSecurityPolicyResponseHeaders cached = _ZNK7WebCore21ContentSecurityPolicy15responseHeadersEv(policy.get());
        Require(cached.headers.size == 3 && cached.httpStatusCode == 404);
    }
    policy.Receive({{",a,,b,", true}}, true, 201);
    Require(!policy.get()->hasCachedResponseHeaders);
    Require(policy.get()->referrer.impl == nullptr);
    {
        const LegacyContentSecurityPolicyResponseHeaders headers = _ZNK7WebCore21ContentSecurityPolicy15responseHeadersEv(policy.get());
        Require(headers.headers.size == 7 && headers.httpStatusCode == 201);
        Require(headers.headers[3].text.impl != nullptr && headers.headers[3].text.impl->length == 0);
        Require(Narrow(headers.headers[4].text.impl) == "a");
        Require(headers.headers[5].text.impl != nullptr && headers.headers[5].text.impl->length == 0);
        Require(Narrow(headers.headers[6].text.impl) == "b");
    }
    {
        Policy empty;
        const LegacyContentSecurityPolicyResponseHeaders headers = _ZNK7WebCore21ContentSecurityPolicy15responseHeadersEv(empty.get());
        Require(headers.headers.size == 0 && headers.headers.buffer == nullptr && headers.httpStatusCode == 0);
        Require(!_ZNK7WebCore21ContentSecurityPolicy22overridesXFrameOptionsEv(empty.get()));
        empty.Receive({{"", true}});
        Require(empty.get()->policies.size == 0);
    }
    {
        Policy withoutFrameAncestors;
        withoutFrameAncestors.Receive({{"default-src 'self'", true}});
        Require(!_ZNK7WebCore21ContentSecurityPolicy22overridesXFrameOptionsEv(withoutFrameAncestors.get()));
    }
    {
        String kept = Make("https://kept.example/");
        StringImpl* keptImpl = kept.impl;
        keptImpl->refCount += 2;
        const unsigned keptCount = keptImpl->refCount;
        {
            Policy shortLived;
            LegacyContentSecurityPolicyResponseHeaders received;
            _ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE(shortLived.get(), &received, &kept, true);
            Require(keptImpl->refCount == keptCount);
        }
        Require(keptImpl->refCount == keptCount - 2);
        NKWebKit::WtfAbi::Release(keptImpl);
    }
    {
        Policy reportOnly;
        reportOnly.Receive({{"frame-ancestors 'none'; report-uri /r", false}});
        Require(!_ZNK7WebCore21ContentSecurityPolicy22overridesXFrameOptionsEv(reportOnly.get()));
        Require(Only(reportOnly.client.messages, "The Content Security Policy directive 'frame-ancestors' is ignored when delivered in a report-only policy."));
    }
    LegacyContentSecurityPolicyResponseHeaders received;
    String given;
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE(nullptr, &received, &given, true); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE(policy.get(), nullptr, &given, true); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE(policy.get(), &received, nullptr, true); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore21ContentSecurityPolicy15responseHeadersEv(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore21ContentSecurityPolicy22overridesXFrameOptionsEv(nullptr); }));
}

static void TestDirectiveMessages() {
    Require(MessagesFor("default-src 'self'; script-src https://a.example:* *.b.example/p/ 'unsafe-inline' 'unsafe-eval' 'nonce-abc=' https: *; object-src 'none'; report-uri /r").empty());
    Require(Only(MessagesFor("allow *"), "The 'allow' directive has been replaced with 'default-src'. Please use that directive instead, as 'allow' has no effect."));
    Require(Only(MessagesFor("OPTIONS x"), "The 'options' directive has been replaced with 'unsafe-inline' and 'unsafe-eval' source expressions for the 'script-src' and 'style-src' directives. Please use those directives instead, as 'options' has no effect."));
    Require(Only(MessagesFor("policy-uri x"), "The 'policy-uri' directive has been removed from the specification. Please specify a complete policy via the Content-Security-Policy header."));
    Require(Only(MessagesFor("foo-bar x"), "Unrecognized Content-Security-Policy directive 'foo-bar'.\n"));
    Require(Only(MessagesFor("manifest-src 'self'"), "Unrecognized Content-Security-Policy directive 'manifest-src'.\n"));
    Require(Only(MessagesFor("@bad x; default-src 'self'"), "Unrecognized Content-Security-Policy directive '@bad'.\n"));
    Require(Only(MessagesFor("default-src:'self' x"), "Unrecognized Content-Security-Policy directive 'default-src:'self''.\n"));
    Require(Only(MessagesFor("DEFAULT-SRC 'self'; default-src *"), "Ignoring duplicate Content-Security-Policy directive 'default-src'.\n"));
    Require(Only(MessagesFor("script-src \xE9 x"), "The value for Content Security Policy directive 'script-src' contains an invalid character: '\xE9 x'. Non-whitespace characters outside ASCII 0x21-0x7E must be percent-encoded, as described in RFC 3986, section 2.1: http://tools.ietf.org/html/rfc3986#section-2.1."));
    Require(Only(MessagesFor("report-uri /a /b; report-uri /c"), "Ignoring duplicate Content-Security-Policy directive 'report-uri'.\n"));
    Require(Only(MessagesFor("upgrade-insecure-requests; upgrade-insecure-requests"), "Ignoring duplicate Content-Security-Policy directive 'upgrade-insecure-requests'.\n"));
    Require(Only(MessagesFor("block-all-mixed-content; block-all-mixed-content"), "Ignoring duplicate Content-Security-Policy directive 'block-all-mixed-content'.\n"));
    Require(MessagesFor(" ; ;default-src 'self';").empty());
    const std::vector<std::string> reportOnly = MessagesFor("sandbox; upgrade-insecure-requests; frame-ancestors 'none'; report-uri /r", false);
    Require(reportOnly.size() == 3);
    Require(reportOnly[0] == "The Content Security Policy directive 'sandbox' is ignored when delivered in a report-only policy.");
    Require(reportOnly[1] == "The Content Security Policy directive 'upgrade-insecure-requests' is ignored when delivered in a report-only policy.");
    Require(reportOnly[2] == "The Content Security Policy directive 'frame-ancestors' is ignored when delivered in a report-only policy.");
    {
        Policy silent;
        silent.Receive({{"foo x; sandbox bogus; upgrade-insecure-requests", true}}, false);
        Require(silent.client.messages.empty());
        Require(silent.get()->isReportingEnabled);
        Require(silent.get()->upgradeInsecureRequests);
        Require(silent.get()->sandboxFlags == -1);
    }
    {
        Policy api;
        api.get()->hasAPIPolicy = true;
        api.Receive({{"default-src 'self'", true}}, true, 204, "https://referrer.example/");
        Require(api.get()->policies.size == 0);
        Require(api.get()->httpStatusCode == 204);
        Require(Narrow(api.get()->referrer.impl) == "https://referrer.example/");
    }
    {
        Policy noClient("https://example.com/", false);
        noClient.Receive({{"foo x", true}});
        Require(noClient.get()->policies.size == 1);
    }
}

static void TestSourceListMessages() {
    const std::vector<std::string> none = MessagesFor("script-src 'none' https://a.example");
    Require(Only(none, "The source list for Content Security Policy directive 'script-src' contains an invalid source: ''none''. It will be ignored. Note that 'none' has no effect unless it is the only expression in the source list."));
    Require(MessagesFor("script-src  'NONE'  ").empty());
    Require(Only(MessagesFor("script-src https://a.example/p?x=1"), "The source list for Content Security Policy directive 'script-src' contains a source with an invalid path: '/p?x=1'. The query component, including the '?', will be ignored."));
    Require(Only(MessagesFor("img-src a.example/p#f"), "The source list for Content Security Policy directive 'img-src' contains a source with an invalid path: '/p#f'. The fragment identifier, including the '#', will be ignored."));
    Require(Only(MessagesFor("script-src img-src"), "The Content Security Policy directive 'script-src' contains 'img-src' as a source expression. Did you mean 'script-src ...; img-src...' (note the semicolon)?"));
    const char* invalid[] = {"https:/p", ".a.example", "a.example..", "ht!tp://x", "https://", "https:///p", "a..b", "*a.example", "a.example:80x", "a.example:65536", "a.example:99999999999999999999", "1http://a.example", "'nonce-'", "'nonce-abc", "'sha256-!!'", "'sha256-AAAAA'", "'sha256-++++='", "'sha256-'", "'md5-AAAA'", "'self", "*.*"};
    for (const char* source : invalid) {
        const std::string expected = std::string("The source list for Content Security Policy directive 'style-src' contains an invalid source: '") + source + "'. It will be ignored.";
        Require(Only(MessagesFor(std::string("style-src ") + source), expected));
    }
    const char* valid[] = {"a.example.", "example.", "*.a.example.", "https://a.example./p", "*", "'self'", "'SELF'", "https:", "a.example:", "a.example", "*.a.example", "a.example:*", "a.example:0", "a.example:65535", "a.example:00080", "https://a.example:443/p/q", "a-b.example/", "custom+scheme-1.x://h", "'nonce-abc+/-_='", "'sha256-47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU='", "'sha256-47DEQpj8HBSa-_TImW-5JCeuQeRkm5NMpJWZG3hSuFU'", "'SHA384-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'", "*:*"};
    for (const char* source : valid) Require(MessagesFor(std::string("style-src ") + source).empty());
    const std::string tooLong = "'sha512-" + std::string(88, 'A') + "'";
    Require(MessagesFor("style-src " + tooLong).size() == 1);
    const std::string longest = "'sha512-" + std::string(86, 'A') + "=='";
    Require(MessagesFor("style-src " + longest).empty());
    {
        Policy policy;
        policy.Receive({{"script-src 'sha256-47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU='; style-src " + longest, true}});
        Require(policy.get()->hashAlgorithmsForInlineScripts == 1);
        Require(policy.get()->hashAlgorithmsForInlineStylesheets == 4);
        policy.Receive({{"default-src 'sha384-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'; default-src 'sha512-AAAA'", true}});
        Require(policy.get()->hashAlgorithmsForInlineScripts == 3);
        Require(policy.get()->hashAlgorithmsForInlineStylesheets == 6);
    }
}

static void TestPluginTypesAndSandbox() {
    Require(Only(MessagesFor("plugin-types"), "'plugin-types' Content Security Policy directive is empty; all plugins will be blocked.\n"));
    Require(MessagesFor("plugin-types application/pdf  image/svg+xml").empty());
    const std::vector<std::string> plugins = MessagesFor("plugin-types application/pdf bad /x a/ a/b/c");
    Require(plugins.size() == 4);
    Require(plugins[0] == "Invalid plugin type in 'plugin-types' Content Security Policy directive: 'bad'.\n");
    Require(plugins[1] == "Invalid plugin type in 'plugin-types' Content Security Policy directive: '/x'.\n");
    Require(plugins[2] == "Invalid plugin type in 'plugin-types' Content Security Policy directive: 'a/'.\n");
    Require(plugins[3] == "Invalid plugin type in 'plugin-types' Content Security Policy directive: 'a/b/c'.\n");
    Require(Only(MessagesFor("plugin-types a/b; plugin-types c/d"), "Ignoring duplicate Content-Security-Policy directive 'plugin-types'.\n"));
    {
        Policy policy;
        policy.Receive({{"sandbox allow-scripts bogus ALLOW-FORMS nope", true}});
        Require(Only(policy.client.messages, "Error while parsing the 'sandbox' Content Security Policy directive: 'bogus', 'nope' are invalid sandbox flags."));
        Require(policy.get()->sandboxFlags == (-1 & ~(1 << 4) & ~(1 << 7) & ~(1 << 3)));
    }
    {
        Policy policy;
        policy.Receive({{"sandbox allow-same-origin allow-top-navigation allow-popups allow-pointer-lock allow-popups-to-escape-sandbox allow-modals allow-storage-access-by-user-activation x", true}});
        Require(Only(policy.client.messages, "Error while parsing the 'sandbox' Content Security Policy directive: 'x' is an invalid sandbox flag."));
        Require(policy.get()->sandboxFlags == (-1 & ~(1 << 2) & ~(1 << 5) & ~(1 << 10) & ~(1 << 6) & ~(1 << 8) & ~(1 << 9) & ~(1 << 12) & ~(1 << 13)));
    }
    {
        Policy policy;
        policy.Receive({{"sandbox allow-top-navigation-by-user-activation", true}, {"sandbox", true}});
        Require(policy.client.messages.empty());
        Require(policy.get()->sandboxFlags == -1);
    }
    {
        Policy policy;
        policy.Receive({{"sandbox allow-forms" "allow-scripts", true}});
        Require(Only(policy.client.messages, "Error while parsing the 'sandbox' Content Security Policy directive: 'allow-forms" "allow-scripts' is an invalid sandbox flag."));
        Require(policy.get()->sandboxFlags == -1);
    }
    {
        Policy policy;
        policy.Receive({{"sandbox allow-forms; sandbox allow-scripts", true}});
        Require(Only(policy.client.messages, "Ignoring duplicate Content-Security-Policy directive 'sandbox'.\n"));
        Require(policy.get()->sandboxFlags == (-1 & ~(1 << 3)));
    }
}

static std::string Upgraded(Policy& policy, std::string_view text, int requestType) {
    URL url = Parse(text);
    _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERN3WTF3URLENS0_19InsecureRequestTypeE(policy.get(), &url, requestType);
    const std::string result = Narrow(url.impl);
    NKWebKit::WtfAbi::Release(url.impl);
    return result;
}

static void TestUpgradeInsecureRequests() {
    Policy plain;
    Require(Upgraded(plain, "http://example.com/a?b#c", 0) == "http://example.com/a?b#c");
    Policy policy;
    policy.Receive({{"upgrade-insecure-requests", true}});
    Require(policy.get()->upgradeInsecureRequests);
    Require(Upgraded(policy, "http://example.com/a?b#c", 0) == "https://example.com/a?b#c");
    Require(Upgraded(policy, "HTTP://example.com:8080/a", 1) == "https://example.com:8080/a");
    Require(Upgraded(policy, "ws://example.com/socket", 0) == "wss://example.com/socket");
    Require(Upgraded(policy, "http://example.com/a", 2) == "http://example.com/a");
    Require(Upgraded(policy, "https://example.com/a", 0) == "https://example.com/a");
    Require(Upgraded(policy, "wss://example.com/a", 0) == "wss://example.com/a");
    Require(Upgraded(policy, "ftp://example.com/a", 0) == "ftp://example.com/a");
    Require(Upgraded(policy, "http://user:pass@example.com/a", 1) == "https://user:pass@example.com/a");
    {
        URL invalid{};
        _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERN3WTF3URLENS0_19InsecureRequestTypeE(policy.get(), &invalid, 0);
        Require(invalid.impl == nullptr && invalid.bits == 0);
    }
    {
        const String text = Make("http://example.com:80/p");
        URL explicitPort{};
        explicitPort.impl = text.impl;
        text.impl->refCount += 2;
        explicitPort.bits = 1u | (1u << 1) | (3u << 3) | (4u << 6);
        explicitPort.userStart = 7;
        explicitPort.userEnd = 7;
        explicitPort.passwordEnd = 7;
        explicitPort.hostEnd = 18;
        explicitPort.pathAfterLastSlash = 22;
        explicitPort.pathEnd = 23;
        explicitPort.queryEnd = 23;
        _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERN3WTF3URLENS0_19InsecureRequestTypeE(policy.get(), &explicitPort, 0);
        Require(Narrow(explicitPort.impl) == "https://example.com/p");
        NKWebKit::WtfAbi::Release(explicitPort.impl);
    }
    {
        Policy navigation;
        constexpr unsigned tableSize = 8;
        auto* metadata = static_cast<unsigned*>(_ZN3WTF10fastMallocEm(4 * sizeof(unsigned) + tableSize * sizeof(LegacySecurityOriginData)));
        std::memset(metadata, 0, 4 * sizeof(unsigned) + tableSize * sizeof(LegacySecurityOriginData));
        metadata[1] = 2;
        metadata[2] = tableSize - 1;
        metadata[3] = tableSize;
        auto* table = reinterpret_cast<LegacySecurityOriginData*>(metadata + 4);
        String protocol = Make("http");
        String host = Make("example.com");
        String otherProtocol = Make("ws");
        String otherHost = Make("sockets.example");
        table[2].protocol = std::exchange(protocol.impl, nullptr);
        table[2].host = std::exchange(host.impl, nullptr);
        table[5].protocol = std::exchange(otherProtocol.impl, nullptr);
        table[5].host = std::exchange(otherHost.impl, nullptr);
        table[5].hasPort = true;
        table[5].port = 8080;
        table[6].protocol = reinterpret_cast<StringImpl*>(static_cast<std::intptr_t>(-1));
        navigation.get()->insecureNavigationRequestsToUpgrade = table;
        Require(Upgraded(navigation, "http://EXAMPLE.com/x", 2) == "https://example.com/x");
        Require(Upgraded(navigation, "http://example.com:81/x", 2) == "http://example.com:81/x");
        Require(Upgraded(navigation, "http://other.example/x", 2) == "http://other.example/x");
        Require(Upgraded(navigation, "ws://sockets.example:8080/x", 2) == "wss://sockets.example:8080/x");
        Require(Upgraded(navigation, "ws://sockets.example/x", 2) == "ws://sockets.example/x");
        Require(Upgraded(navigation, "http://other.example/x", 0) == "http://other.example/x");
    }
    {
        LegacyResourceRequestBase request{};
        request.url = Parse("http://example.com/form");
        request.resourceRequestUpdated = true;
        request.platformRequestUpdated = true;
        StringImpl* insecure = request.url.impl;
        insecure->refCount += 2;
        const unsigned insecureCount = insecure->refCount;
        _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE(policy.get(), &request, 1);
        Require(Narrow(request.url.impl) == "https://example.com/form");
        Require(request.resourceRequestUpdated && !request.platformRequestUpdated);
        Require(request.url.impl != insecure && insecure->refCount == insecureCount - 2);
        NKWebKit::WtfAbi::Release(insecure);
        request.platformRequestUpdated = true;
        StringImpl* secure = request.url.impl;
        const unsigned secureCount = secure->refCount;
        _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE(plain.get(), &request, 0);
        Require(Narrow(request.url.impl) == "https://example.com/form");
        Require(!request.platformRequestUpdated);
        Require(request.url.impl == secure && secure->refCount == secureCount);
        _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE(policy.get(), &request, 0);
        Require(request.url.impl == secure && secure->refCount == secureCount);
        request.resourceRequestUpdated = false;
        Require(Throws<std::logic_error>([&] { _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE(policy.get(), &request, 0); }));
        NKWebKit::WtfAbi::Release(request.url.impl);
        Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE(policy.get(), nullptr, 0); }));
        Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE(nullptr, &request, 0); }));
    }
    URL url{};
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERN3WTF3URLENS0_19InsecureRequestTypeE(nullptr, &url, 0); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERN3WTF3URLENS0_19InsecureRequestTypeE(policy.get(), nullptr, 0); }));
}

enum class Kind { Script, Connect, Child };

static bool Allowed(Policy& policy, std::string_view text, bool redirect = false, Kind kind = Kind::Script) {
    URL url = text.empty() ? URL{} : Parse(text);
    bool allowed = false;
    if (kind == Kind::Script)
        allowed = _ZNK7WebCore21ContentSecurityPolicy21allowScriptFromSourceERKN3WTF3URLENS0_24RedirectResponseReceivedE(policy.get(), &url, redirect);
    else if (kind == Kind::Connect)
        allowed = _ZNK7WebCore21ContentSecurityPolicy20allowConnectToSourceERKN3WTF3URLENS0_24RedirectResponseReceivedE(policy.get(), &url, redirect);
    else
        allowed = _ZNK7WebCore21ContentSecurityPolicy27allowChildContextFromSourceERKN3WTF3URLENS0_24RedirectResponseReceivedE(policy.get(), &url, redirect);
    NKWebKit::WtfAbi::Release(url.impl);
    return allowed;
}

static bool Matches(std::string_view sources, std::string_view url, bool redirect = false, std::string_view self = "https://example.com/") {
    Policy policy(self);
    policy.Receive({{"script-src " + std::string(sources), true}});
    Require(policy.client.messages.empty());
    const bool allowed = Allowed(policy, url, redirect);
    Require(policy.client.messages.size() == (allowed ? 0u : 1u));
    Require(policy.client.violations.size() == (allowed ? 0u : 1u));
    Require(policy.client.reports.empty());
    return allowed;
}

static void TestSourceMatching() {
    {
        Policy none;
        Require(Allowed(none, "http://anything.example/a.js"));
        Require(none.client.messages.empty() && none.client.announced.empty() && none.client.violations.empty());
    }
    Require(Matches("'self'", "https://example.com/a.js"));
    Require(!Matches("'self'", "http://example.com/a.js"));
    Require(!Matches("'self'", "https://example.com:444/a.js"));
    Require(!Matches("'self'", "https://other.example/a.js"));
    Require(Matches("'self'", "https://example.com/a.js", false, "http://example.com/"));
    Require(Matches("'self'", "http://example.com:8080/a.js", false, "http://example.com:8080/"));
    Require(!Matches("'self'", "http://example.com/a.js", false, "http://example.com:8080/"));
    Require(!Matches("'self'", "https://example.com/a.js", false, "data:text/html,x"));
    Require(Matches("https://cdn.example", "https://cdn.example/x"));
    Require(Matches("https://cdn.example", "https://CDN.example/x"));
    Require(Matches("https://CDN.Example", "https://cdn.example/x"));
    Require(!Matches("https://cdn.example", "http://cdn.example/x"));
    Require(!Matches("https://cdn.example", "https://cdn.example:8443/x"));
    Require(!Matches("https://cdn.example", "https://cdn.example.org/x"));
    Require(!Matches("https://cdn.example", "https://a.cdn.example/x"));
    Require(Matches("http://cdn.example", "https://cdn.example/x"));
    Require(Matches("HTTP://cdn.example", "https://cdn.example/x"));
    Require(Matches("cdn.example", "https://cdn.example/x"));
    Require(!Matches("cdn.example", "http://cdn.example/x"));
    Require(Matches("cdn.example", "https://cdn.example/x", false, "http://example.com/"));
    Require(Matches("*.cdn.example", "https://a.cdn.example/x"));
    Require(Matches("*.cdn.example", "https://a.b.cdn.example/x"));
    Require(Matches("*.cdn.example", "https://cdn.example/x"));
    Require(!Matches("*.cdn.example", "https://xcdn.example/x"));
    Require(Matches("https://*", "https://anything.example/x"));
    Require(Matches("https://*/allowed/", "https://anything.example/allowed/x"));
    Require(Matches("https://*/allowed/", "https://anything.example/elsewhere/x"));
    Require(!Matches("https://*/allowed/", "http://anything.example/allowed/x"));
    Require(Matches("https:", "https://anything.example/elsewhere/x"));
    Require(Matches("https://cdn.example/allowed/", "https://cdn.example/allowed/x"));
    Require(!Matches("https://cdn.example/allowed/", "https://cdn.example/elsewhere/x"));
    Require(Matches("cdn.example.", "https://cdn.example./x"));
    Require(!Matches("cdn.example.", "https://cdn.example/x"));
    Require(!Matches("https://*", "http://anything.example/x"));
    Require(Matches("https://cdn.example:8443", "https://cdn.example:8443/x"));
    Require(!Matches("https://cdn.example:8443", "https://cdn.example/x"));
    Require(Matches("https://cdn.example:*", "https://cdn.example:8443/x"));
    Require(Matches("https://cdn.example:*", "https://cdn.example/x"));
    Require(Matches("http://cdn.example:80", "https://cdn.example/x"));
    Require(Matches("http://cdn.example:80", "http://cdn.example/x"));
    Require(!Matches("http://cdn.example:80", "http://cdn.example:81/x"));
    Require(Matches("https://cdn.example:443", "https://cdn.example/x"));
    Require(!Matches("https://cdn.example:444", "https://cdn.example/x"));
    Require(Matches("cdn.example:80", "http://cdn.example/x", false, "http://example.com/"));
    Require(Matches("https://cdn.example/js/", "https://cdn.example/js/a.js"));
    Require(Matches("https://cdn.example/js/", "https://cdn.example/js/"));
    Require(!Matches("https://cdn.example/js/", "https://cdn.example/js"));
    Require(!Matches("https://cdn.example/js/", "https://cdn.example/other/a.js"));
    Require(Matches("https://cdn.example/js/", "https://cdn.example/other/a.js", true));
    Require(!Matches("https://other.example/js/", "https://cdn.example/js/a.js", true));
    Require(Matches("https://cdn.example/js/app.js", "https://cdn.example/js/app.js"));
    Require(Matches("https://cdn.example/js/app.js", "https://cdn.example/js/app.js?x=1#f"));
    Require(!Matches("https://cdn.example/js/app.js", "https://cdn.example/js/app.jsx"));
    Require(!Matches("https://cdn.example/js/app.js", "https://cdn.example/js/APP.js"));
    Require(Matches("https://cdn.example/a%20b/", "https://cdn.example/a%20b/c"));
    Require(Matches("https://cdn.example/caf%C3%A9/", "https://cdn.example/caf%c3%a9/x"));
    Require(!Matches("https://cdn.example/caf%C3%A9/", "https://cdn.example/caf%C3%A8/x"));
    Require(Matches("https://cdn.example/%FF%41/", "https://cdn.example/%EF%BF%BDA/x"));
    Require(Matches("https://cdn.example/%E2%41/", "https://cdn.example/%EF%BF%BD/x"));
    Require(Matches("https://cdn.example/%E2%28%A1/", "https://cdn.example/%EF%BF%BD(%EF%BF%BD/x"));
    Require(Matches("https://cdn.example/%F0%9F%98%80/", "https://cdn.example/%F0%9F%98%80/x"));
    Require(!Matches("https://cdn.example/%F0%9F%98%80/", "https://cdn.example/%EF%98%80/x"));
    Require(Matches("https://cdn.example/%E2%82%28/", "https://cdn.example/%EF%BF%BD(/x"));
    Require(!Matches("https://cdn.example/%E2%82%28/", "https://cdn.example/%EF%BF%BD%EF%BF%BD(/x"));
    Require(Matches("https://cdn.example/%E2A/", "https://cdn.example/%EF%BF%BD/x"));
    Require(!Matches("https://cdn.example/%E2A/", "https://cdn.example/%EF%BF%BDA/x"));
    Require(!Matches("https://cdn.example/%E2%41/", "https://cdn.example/%EF%BF%BDA/x"));
    Require(Matches("https://cdn.example/%ED%A0%80/", "https://cdn.example/%EF%BF%BD%EF%BF%BD%EF%BF%BD/x"));
    Require(Matches("https://cdn.example/100%/", "https://cdn.example/100%/x"));
    Require(Matches("https://cdn.example/a%2/", "https://cdn.example/a%2/x"));
    Require(Matches("https:", "https://anything.example/x"));
    Require(!Matches("https:", "http://anything.example/x"));
    Require(Matches("http:", "https://anything.example/x"));
    Require(Matches("data:", "data:text/javascript,1"));
    Require(!Matches("https:", "data:text/javascript,1"));
    Require(Matches("*", "https://anything.example/x"));
    Require(Matches("*", "http://anything.example/x"));
    Require(Matches("*", "ws://anything.example/x"));
    Require(Matches("*", "wss://anything.example/x"));
    Require(!Matches("*", "data:text/javascript,1"));
    Require(!Matches("*", "custom://host/x"));
    Require(Matches("*", "custom://host/x", false, "custom://self/"));
    Require(!Matches("'none'", "https://example.com/a.js"));
    Require(!Matches("'unsafe-inline' 'unsafe-eval' 'nonce-abc'", "https://example.com/a.js"));
    Require(!Matches("*", ""));
    Require(!Matches("'self'", "", false, "data:text/html,x"));
    Require(Matches("'self'", "https://example.com/a.js", false, "blob:https://example.com/1234-5678"));
    Require(!Matches("'self'", "https://other.example/a.js", false, "blob:https://example.com/1234-5678"));
    {
        Policy blob("blob:https://example.com:8443/1234-5678");
        Require(Narrow(blob.get()->selfSourceProtocol.impl) == "https");
        Policy custom("custom://self/");
        Require(Narrow(custom.get()->selfSourceProtocol.impl) == "custom");
    }
    {
        Policy policy;
        policy.Receive({{"script-src 'none'", true}});
        const String scheme = Make("ByPass");
        _ZN7WebCore20LegacySchemeRegistry49registerURLSchemeAsBypassingContentSecurityPolicyERKN3WTF6StringE(scheme);
        Require(Allowed(policy, "bypass://host/x"));
        Require(Allowed(policy, "bypass://host/x", false, Kind::Connect));
        Require(policy.client.messages.empty());
        _ZN7WebCore20LegacySchemeRegistry57removeURLSchemeRegisteredAsBypassingContentSecurityPolicyERKN3WTF6StringE(scheme);
        Require(!Allowed(policy, "bypass://host/x"));
    }
}

static void TestViolationMessages() {
    {
        Policy policy;
        policy.Receive({{"default-src 'self'", true}});
        Require(!Allowed(policy, "https://other.example/a.js"));
        Require(Only(policy.client.messages, "Refused to load https://other.example/a.js because it appears in neither the script-src directive nor the default-src directive of the Content Security Policy."));
        Require(policy.client.sources[0] == 8 && policy.client.levels[0] == 2 && policy.client.requestIdentifiers[0] == 0);
        Require(policy.client.violations[0].violatedDirective == "default-src 'self'" && policy.client.violations[0].effectiveDirective == "script-src");
    }
    {
        Policy policy;
        policy.Receive({{"default-src 'none'; connect-src 'self'; child-src https://workers.example", true}});
        Require(Allowed(policy, "https://example.com/api", false, Kind::Connect));
        Require(Allowed(policy, "https://workers.example/w.js", false, Kind::Child));
        Require(!Allowed(policy, "wss://example.com/socket", false, Kind::Connect));
        Require(!Allowed(policy, "https://example.com/w.js", false, Kind::Child));
        Require(!Allowed(policy, "https://example.com/a.js"));
        Require(policy.client.messages.size() == 3);
        Require(policy.client.messages[0] == "Refused to connect to wss://example.com/socket because it does not appear in the connect-src directive of the Content Security Policy.");
        Require(policy.client.messages[1] == "Refused to load https://example.com/w.js because it does not appear in the child-src directive of the Content Security Policy.");
        Require(policy.client.messages[2] == "Refused to load https://example.com/a.js because it appears in neither the script-src directive nor the default-src directive of the Content Security Policy.");
        Require(policy.client.violations[0].effectiveDirective == "connect-src" && policy.client.violations[1].effectiveDirective == "child-src");
    }
    {
        Policy policy;
        policy.Receive({{"script-src 'self'; report-uri /r", false}, {"script-src https:", true}});
        Require(Allowed(policy, "https://other.example/a.js"));
        Require(Only(policy.client.messages, "[Report Only] Refused to load https://other.example/a.js because it does not appear in the script-src directive of the Content Security Policy."));
        Require(policy.client.reports.size() == 1);
        Require(!Allowed(policy, "http://other.example/a.js"));
        Require(policy.client.messages.size() == 3 && policy.client.violations.size() == 3);
        Require(policy.client.messages[2] == "Refused to load http://other.example/a.js because it does not appear in the script-src directive of the Content Security Policy.");
    }
    {
        Policy policy;
        policy.Receive({{"script-src *", true}});
        URL empty{};
        Require(!_ZNK7WebCore21ContentSecurityPolicy21allowScriptFromSourceERKN3WTF3URLENS0_24RedirectResponseReceivedE(policy.get(), &empty, false));
        Require(Only(policy.client.messages, "Refused to load because it does not appear in the script-src directive of the Content Security Policy."));
        Require(policy.client.announced[0].empty() && policy.client.violations[0].blockedURI.empty());
    }
    {
        Policy silenced;
        silenced.Receive({{"script-src 'none'; report-uri /r", true}});
        silenced.get()->isReportingEnabled = false;
        Require(!Allowed(silenced, "https://example.com/a.js"));
        Require(silenced.client.messages.empty() && silenced.client.announced.empty() && silenced.client.violations.empty() && silenced.client.reports.empty());
        silenced.get()->isReportingEnabled = true;
    }
    {
        Policy noClient("https://example.com/", false);
        noClient.Receive({{"script-src 'none'; report-uri /r", true}});
        Require(!Allowed(noClient, "https://example.com/a.js"));
    }
    Policy policy;
    URL url{};
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy21allowScriptFromSourceERKN3WTF3URLENS0_24RedirectResponseReceivedE(nullptr, &url, false); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy21allowScriptFromSourceERKN3WTF3URLENS0_24RedirectResponseReceivedE(policy.get(), nullptr, false); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy20allowConnectToSourceERKN3WTF3URLENS0_24RedirectResponseReceivedE(nullptr, &url, false); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy27allowChildContextFromSourceERKN3WTF3URLENS0_24RedirectResponseReceivedE(policy.get(), nullptr, false); }));
}

static void TestViolationReports() {
    const std::string header = "script-src http://example.com/js/; report-uri /csp https://reports.example/r";
    {
        Policy policy("http://example.com/page");
        policy.Receive({{header, true}}, true, 418, "http://ref.example/");
        Require(!Allowed(policy, "http://evil.example/x.js?token=1#frag"));
        Require(policy.client.announced.size() == 1 && policy.client.announced[0] == "http://evil.example/x.js?token=1#frag");
        Require(policy.client.violations.size() == 1);
        const RecordedViolation& violation = policy.client.violations[0];
        Require(violation.documentURI == "http://evil.example/x.js?token=1#frag");
        Require(violation.referrer == "http://ref.example/");
        Require(violation.blockedURI == "http://evil.example");
        Require(violation.violatedDirective == "script-src http://example.com/js/");
        Require(violation.effectiveDirective == "script-src");
        Require(violation.originalPolicy == header);
        Require(violation.sourceFileIsNull && violation.statusCode == 418 && violation.lineNumber == 0 && violation.columnNumber == 0 && !violation.flags);
        Require(policy.client.reports.size() == 2);
        Require(policy.client.reports[0].url == "http://example.com/csp");
        Require(policy.client.reports[1].url == "https://reports.example/r");
        const std::string body = "{\"csp-report\":{\"document-uri\":\"http://evil.example/x.js?token=1#frag\",\"referrer\":\"http://ref.example/\",\"violated-directive\":\"script-src http://example.com/js/\",\"effective-directive\":\"script-src\",\"original-policy\":\"" + header + "\",\"blocked-uri\":\"http://evil.example\",\"status-code\":418}}";
        Require(policy.client.reports[0].body == body && policy.client.reports[1].body == body);
        Require(policy.client.reports[0].refCount == 2 && policy.client.reports[1].refCount == 2);
        Require(policy.client.reports[0].bytesCapacity == body.size() && policy.client.reports[0].elementsCapacity == 16);
        Require(!Allowed(policy, "http://user:pass@example.com/other/a.js?q=1#f"));
        Require(policy.client.violations[1].blockedURI == "http://example.com/other/a.js?q=1");
        Require(!Allowed(policy, "data:text/javascript,1"));
        Require(policy.client.violations[2].blockedURI == "data");
        Require(!Allowed(policy, "http://example.com:8080/a.js"));
        Require(policy.client.violations[3].blockedURI == "http://example.com:8080");
    }
    {
        Policy policy("http://example.com/page");
        policy.client.setSourceFile = true;
        policy.client.takeReport = true;
        policy.Receive({{"script-src 'none'; report-uri r", true}}, true, 200);
        Require(!Allowed(policy, "http://example.com/a.js"));
        const RecordedViolation& violation = policy.client.violations[0];
        Require(violation.sourceFile == "https://example.com/script.js" && violation.lineNumber == 12 && violation.columnNumber == 34);
        Require(violation.referrer.empty() && violation.statusCode == 200);
        Require(policy.client.reports.size() == 1 && policy.client.reports[0].url == "http://example.com/r");
        Require(policy.client.reports[0].body == "{\"csp-report\":{\"document-uri\":\"http://example.com/a.js\",\"referrer\":\"\",\"violated-directive\":\"script-src 'none'\",\"effective-directive\":\"script-src\",\"original-policy\":\"script-src 'none'; report-uri r\",\"blocked-uri\":\"http://example.com/a.js\",\"status-code\":200,\"source-file\":\"https://example.com/script.js\",\"line-number\":12,\"column-number\":34}}");
        Require(policy.client.reports[0].bytesCapacity == policy.client.reports[0].body.size());
        Require(policy.client.taken.size() == 1 && policy.client.taken[0]->refCount == 1);
        auto* kept = reinterpret_cast<NKWebKit::WebCoreAbi::FormData*>(policy.client.taken[0]);
        _ZN7WebCore8FormDataD1Ev(kept);
        _ZN3WTF8fastFreeEPv(kept);
    }
    {
        Policy secure;
        secure.Receive({{"script-src 'none'; report-uri /r", true}}, true, 404, "a\"b\\c\n\x01");
        Require(!Allowed(secure, "https://example.com/a.js"));
        Require(secure.client.violations[0].statusCode == 0);
        Require(secure.client.reports[0].body.find("\"referrer\":\"a\\\"b\\\\c\\n\\u0001\",") != std::string::npos);
        Require(secure.client.reports[0].body.find("\"status-code\":0}}") != std::string::npos);
        Require(secure.client.reports[0].bytesCapacity == secure.client.reports[0].body.size());
    }
    {
        Policy upperCase("custom://HOST/");
        upperCase.Receive({{"script-src 'none'", true}});
        Require(!Allowed(upperCase, "custom://host/a.js?q#f"));
        Require(upperCase.client.violations[0].blockedURI == "custom://host/a.js?q");
    }
    {
        Policy tiny;
        tiny.Receive({{"script-src 'none'", true}});
        Require(!Allowed(tiny, "https://example.com/a.js"));
        Require(tiny.client.violations.size() == 1 && tiny.client.reports.empty());
    }
}

struct TestOrigin {
    LegacySecurityOrigin origin{};
    String protocol;
    String host;

    TestOrigin(std::string_view protocolText, std::string_view hostText, int port = -1) : protocol(Make(protocolText)), host(Make(hostText)) {
        origin.refCount = 1;
        origin.protocol = protocol.impl;
        origin.host = host.impl;
        origin.hasPort = port >= 0;
        origin.port = port >= 0 ? static_cast<std::uint16_t>(port) : 0;
    }
};

static bool FrameAllowed(Policy& policy, std::initializer_list<TestOrigin*> ancestors, bool overridePolicy = false) {
    std::vector<LegacySecurityOrigin*> pointers;
    for (TestOrigin* ancestor : ancestors) pointers.push_back(&ancestor->origin);
    LegacySecurityOriginVector vector{pointers.data(), static_cast<unsigned>(pointers.size()), static_cast<unsigned>(pointers.size())};
    URL url = Parse("https://example.com/framed");
    const bool allowed = _ZNK7WebCore21ContentSecurityPolicy19allowFrameAncestorsERKN3WTF6VectorINS1_6RefPtrINS_14SecurityOriginENS1_13DumbPtrTraitsIS4_EEEELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLEb(policy.get(), &vector, &url, overridePolicy);
    NKWebKit::WtfAbi::Release(url.impl);
    return allowed;
}

static void TestFrameAncestors() {
    TestOrigin self("https", "example.com");
    TestOrigin parent("https", "parent.example");
    TestOrigin parentWithPort("https", "parent.example", 8443);
    TestOrigin stranger("https", "stranger.example");
    TestOrigin plain("http", "parent.example");
    TestOrigin file("file", "");
    TestOrigin separated("file", "");
    separated.origin.enforcesFilePathSeparation = true;
    TestOrigin unique("https", "parent.example");
    unique.origin.isUnique = true;
    Policy policy;
    policy.Receive({{"frame-ancestors 'self' https://parent.example file:", true}});
    Require(FrameAllowed(policy, {}));
    Require(FrameAllowed(policy, {&self}));
    Require(FrameAllowed(policy, {&parent, &self}));
    Require(FrameAllowed(policy, {&file}));
    Require(policy.client.messages.empty());
    Require(!FrameAllowed(policy, {&parent, &stranger}));
    Require(Only(policy.client.messages, "Refused to load https://example.com/framed because it does not appear in the frame-ancestors directive of the Content Security Policy."));
    Require(policy.client.violations.size() == 1 && policy.client.violations[0].effectiveDirective == "frame-ancestors");
    Require(policy.client.violations[0].violatedDirective == "frame-ancestors 'self' https://parent.example file:");
    Require(policy.client.violations[0].blockedURI == "https://example.com/framed");
    Require(!FrameAllowed(policy, {&parentWithPort}));
    Require(!FrameAllowed(policy, {&plain}));
    Require(!FrameAllowed(policy, {&unique}));
    Require(!FrameAllowed(policy, {&separated}));
    Require(policy.client.messages.size() == 5);
    Require(FrameAllowed(policy, {&stranger}, true));
    Require(policy.client.messages.size() == 5);
    {
        Policy withPort;
        withPort.Receive({{"frame-ancestors https://parent.example:8443", true}});
        Require(FrameAllowed(withPort, {&parentWithPort}));
        Require(!FrameAllowed(withPort, {&parent}));
    }
    {
        Policy without;
        without.Receive({{"default-src 'none'", true}});
        Require(FrameAllowed(without, {&stranger}));
        Require(without.client.messages.empty());
    }
    LegacySecurityOriginVector empty{nullptr, 0, 0};
    LegacySecurityOrigin* missing = nullptr;
    LegacySecurityOriginVector withNull{&missing, 1, 1};
    URL url{};
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy19allowFrameAncestorsERKN3WTF6VectorINS1_6RefPtrINS_14SecurityOriginENS1_13DumbPtrTraitsIS4_EEEELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLEb(nullptr, &empty, &url, false); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy19allowFrameAncestorsERKN3WTF6VectorINS1_6RefPtrINS_14SecurityOriginENS1_13DumbPtrTraitsIS4_EEEELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLEb(policy.get(), nullptr, &url, false); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy19allowFrameAncestorsERKN3WTF6VectorINS1_6RefPtrINS_14SecurityOriginENS1_13DumbPtrTraitsIS4_EEEELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLEb(policy.get(), &empty, nullptr, false); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore21ContentSecurityPolicy19allowFrameAncestorsERKN3WTF6VectorINS1_6RefPtrINS_14SecurityOriginENS1_13DumbPtrTraitsIS4_EEEELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLEb(policy.get(), &withNull, &url, false); }));
}

int main() {
    TestConstruction();
    TestHeaders();
    TestDirectiveMessages();
    TestSourceListMessages();
    TestPluginTypesAndSandbox();
    TestUpgradeInsecureRequests();
    TestSourceMatching();
    TestViolationMessages();
    TestViolationReports();
    TestFrameAncestors();
    std::puts("libSceNKWebKit ContentSecurityPolicy tests passed");
    return 0;
}
