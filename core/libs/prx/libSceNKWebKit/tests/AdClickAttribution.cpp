#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::AdClickAbi::AdClickAttribution;
using NKWebKit::AdClickAbi::Conversion;
using NKWebKit::AdClickAbi::ExpectedConversion;
using NKWebKit::AdClickAbi::OptionalSeconds;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

const auto ParseConversionRequest = &_ZN7WebCore18AdClickAttribution22parseConversionRequestERKN3WTF3URLE;
const auto Convert = &_ZN7WebCore18AdClickAttribution31convertAndGetEarliestTimeToSendEONS0_10ConversionE;
const auto HasHigherPriorityThan = &_ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_;
const auto Url = &_ZNK7WebCore18AdClickAttribution3urlEv;
const auto UrlForTesting = &_ZNK7WebCore18AdClickAttribution13urlForTestingERKN3WTF3URLE;
const auto Referrer = &_ZNK7WebCore18AdClickAttribution8referrerEv;
const auto MarkAsExpired = &_ZN7WebCore18AdClickAttribution13markAsExpiredEv;
const auto HasExpired = &_ZNK7WebCore18AdClickAttribution10hasExpiredEv;
const auto MarkConversionAsSent = &_ZN7WebCore18AdClickAttribution20markConversionAsSentEv;
const auto WasConversionSent = &_ZNK7WebCore18AdClickAttribution17wasConversionSentEv;
const auto ToString = &_ZNK7WebCore18AdClickAttribution8toStringEv;

constexpr double Day = 24.0 * 60.0 * 60.0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "AdClickAttribution check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> bool Throws(TAction action) {
    try {
        action();
    } catch (const TException&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

struct Text {
    std::string narrow;
    std::u16string wide;
    StringImpl impl;

    Text(const std::string& text) : narrow(text), impl{2, static_cast<unsigned>(narrow.size()), narrow.data(), 4} {}
    Text(const std::u16string& text) : wide(text), impl{2, static_cast<unsigned>(wide.size()), wide.data(), 0} {}
    Text(const Text&) = delete;
};

std::u16string Units(const StringImpl* impl) {
    std::u16string text;
    if (!impl) return text;
    for (unsigned index = 0; index < impl->length; ++index) {
        text.push_back(impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(impl->data)[index] : static_cast<const char16_t*>(impl->data)[index]);
    }
    return text;
}

std::u16string Widen(const std::string& text) {
    std::u16string wide;
    for (const unsigned char unit : text) wide.push_back(unit);
    return wide;
}

bool Is(const StringImpl* impl, const std::string& expected) {
    return impl && Units(impl) == Widen(expected);
}

struct OwnedURL {
    URL value;
    OwnedURL(URL url) : value(url) {}
    OwnedURL(const OwnedURL&) = delete;
    ~OwnedURL() { NKWebKit::WtfAbi::Release(value.impl); }
};

bool IsNullURL(const URL& url) {
    const URL null{};
    return !std::memcmp(&url, &null, sizeof(URL));
}

AdClickAttribution Make(std::uint32_t campaign, StringImpl* source, StringImpl* destination) {
    AdClickAttribution attribution{};
    attribution.campaignId = campaign;
    attribution.source = source;
    attribution.destination = destination;
    attribution.timeOfAdClick = _ZN3WTF8WallTime3nowEv().m_value;
    return attribution;
}

void TestConversion() {
    Text source(std::string("source.example"));
    Text destination(std::string("destination.example"));
    AdClickAttribution attribution = Make(12, &source.impl, &destination.impl);
    Require(!WasConversionSent(&attribution));
    MarkConversionAsSent(&attribution);
    Require(!attribution.conversion.engaged && !WasConversionSent(&attribution));

    OptionalSeconds result;
    std::memset(&result, 0xA5, sizeof(result));
    const Conversion tooMuchData{64, 0, false};
    Require(Convert(&result, &attribution, &tooMuchData) == &result);
    Require(!result.engaged && !attribution.conversion.engaged && !attribution.earliestTimeToSend.engaged);
    const unsigned char zeros[16] = {};
    Require(!std::memcmp(&result, zeros, sizeof(result)));
    const Conversion tooMuchPriority{0, 64, false};
    Convert(&result, &attribution, &tooMuchPriority);
    Require(!result.engaged && !attribution.conversion.engaged);

    const double before = _ZN3WTF8WallTime3nowEv().m_value;
    const Conversion first{63, 5, false};
    Convert(&result, &attribution, &first);
    const double after = _ZN3WTF8WallTime3nowEv().m_value;
    Require(result.engaged && result.value >= Day && result.value < 2.0 * Day);
    Require(attribution.conversion.engaged && attribution.conversion.value.data == 63 && attribution.conversion.value.priority == 5 && !attribution.conversion.value.wasSent);
    Require(attribution.earliestTimeToSend.engaged);
    Require(attribution.earliestTimeToSend.value >= before + result.value && attribution.earliestTimeToSend.value <= after + result.value);

    const double kept = attribution.earliestTimeToSend.value;
    const Conversion samePriority{1, 5, true};
    Convert(&result, &attribution, &samePriority);
    Require(!result.engaged && attribution.conversion.value.data == 63 && attribution.earliestTimeToSend.value == kept);
    const Conversion lowerPriority{1, 4, true};
    Convert(&result, &attribution, &lowerPriority);
    Require(!result.engaged && attribution.conversion.value.data == 63);

    const Conversion higherPriority{2, 6, true};
    Convert(&result, &attribution, &higherPriority);
    Require(result.engaged && attribution.conversion.value.data == 2 && attribution.conversion.value.priority == 6 && attribution.conversion.value.wasSent);
    Require(WasConversionSent(&attribution));

    attribution.conversion.value.wasSent = false;
    Require(!WasConversionSent(&attribution));
    MarkConversionAsSent(&attribution);
    Require(WasConversionSent(&attribution));

    bool varied = false;
    double previous = -1.0;
    for (std::uint32_t priority = 7; priority < 39; ++priority) {
        const Conversion next{0, priority, false};
        Convert(&result, &attribution, &next);
        Require(result.engaged && result.value >= Day && result.value < 2.0 * Day);
        if (previous >= 0.0 && previous != result.value) varied = true;
        previous = result.value;
    }
    Require(varied);

    Require(Throws<std::invalid_argument>([&] { Convert(nullptr, &attribution, &first); }));
    Require(Throws<std::invalid_argument>([&] { Convert(&result, nullptr, &first); }));
    Require(Throws<std::invalid_argument>([&] { Convert(&result, &attribution, nullptr); }));
    Require(Throws<std::invalid_argument>([] { MarkConversionAsSent(nullptr); }));
    Require(Throws<std::invalid_argument>([] { WasConversionSent(nullptr); }));
}

void TestPriority() {
    AdClickAttribution a{};
    AdClickAttribution b{};
    Require(HasHigherPriorityThan(&a, &b));
    b.conversion = {true, {0, 3, false}};
    Require(!HasHigherPriorityThan(&a, &b));
    Require(HasHigherPriorityThan(&b, &a));
    a.conversion = {true, {0, 3, false}};
    Require(!HasHigherPriorityThan(&a, &b));
    a.conversion.value.priority = 4;
    Require(HasHigherPriorityThan(&a, &b));
    Require(!HasHigherPriorityThan(&b, &a));
    Require(Throws<std::invalid_argument>([&] { HasHigherPriorityThan(nullptr, &b); }));
    Require(Throws<std::invalid_argument>([&] { HasHigherPriorityThan(&a, nullptr); }));
}

void TestExpiry() {
    AdClickAttribution attribution{};
    const double now = _ZN3WTF8WallTime3nowEv().m_value;
    attribution.timeOfAdClick = now;
    Require(!HasExpired(&attribution));
    attribution.timeOfAdClick = now - 6.0 * Day;
    Require(!HasExpired(&attribution));
    attribution.timeOfAdClick = now - 8.0 * Day;
    Require(HasExpired(&attribution));
    attribution.timeOfAdClick = now;
    MarkAsExpired(&attribution);
    Require(attribution.timeOfAdClick == 0.0 && HasExpired(&attribution));
    Require(Throws<std::invalid_argument>([] { MarkAsExpired(nullptr); }));
    Require(Throws<std::invalid_argument>([] { HasExpired(nullptr); }));
}

void TestUrls() {
    Text source(std::string("source.example"));
    Text destination(std::string("destination.example"));
    AdClickAttribution attribution = Make(12, &source.impl, &destination.impl);
    Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));

    attribution.conversion = {true, {7, 3, false}};
    Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));
    attribution.earliestTimeToSend = {true, 1.0};
    {
        const OwnedURL url(Url(&attribution));
        Require((url.value.bits & 1) && Is(url.value.impl, "https://source.example/.well-known/ad-click-attribution/7/12"));
        const OwnedURL referrer(Referrer(&attribution));
        Require((referrer.value.bits & 1) && Is(referrer.value.impl, "https://destination.example/"));
    }

    attribution.campaignId = 64;
    Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));
    attribution.campaignId = 63;
    attribution.conversion.value.data = 64;
    Require(IsNullURL(Url(&attribution)));
    attribution.conversion.value.data = 63;
    attribution.conversion.value.priority = 64;
    Require(IsNullURL(Referrer(&attribution)));
    attribution.conversion.value.priority = 63;
    {
        const OwnedURL url(Url(&attribution));
        Require(Is(url.value.impl, "https://source.example/.well-known/ad-click-attribution/63/63"));
    }

    Text nullOrigin(std::string("nullOrigin"));
    Text empty(std::string(""));
    for (StringImpl* domain : {static_cast<StringImpl*>(nullptr), &empty.impl, &nullOrigin.impl}) {
        attribution.source = domain;
        Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));
        attribution.source = &source.impl;
        attribution.destination = domain;
        Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));
        attribution.destination = &destination.impl;
    }

    Text unparsable(std::string("bad host:x"));
    attribution.source = &unparsable.impl;
    Require(IsNullURL(Url(&attribution)));
    {
        const OwnedURL referrer(Referrer(&attribution));
        Require(Is(referrer.value.impl, "https://destination.example/"));
    }
    attribution.source = &source.impl;
    attribution.destination = &unparsable.impl;
    Require(IsNullURL(Referrer(&attribution)));

    Text wideSource(std::u16string(u"source.example"));
    attribution.source = &wideSource.impl;
    attribution.destination = &destination.impl;
    {
        const OwnedURL url(Url(&attribution));
        Require((url.value.bits & 1) && Is(url.value.impl, "https://source.example/.well-known/ad-click-attribution/63/63"));
    }
    Require(Throws<std::invalid_argument>([] { Url(nullptr); }));
    Require(Throws<std::invalid_argument>([] { Referrer(nullptr); }));
}

URL ParseBase(Text& input) {
    String string(&input.impl);
    const URL empty{};
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &empty, &string, nullptr);
    string.impl = nullptr;
    return url;
}

void TestUrlForTesting() {
    Text localhost(std::string("localhost"));
    Text loopback(std::string("127.0.0.1"));
    Text other(std::string("source.example"));
    Text destination(std::string("destination.example"));
    Text plainText(std::string("http://127.0.0.1:8000/report"));
    Text withQueryText(std::string("http://127.0.0.1:8000/report?a=b&c"));
    Text withEmptyQueryText(std::string("http://127.0.0.1:8000/report?"));
    const OwnedURL plain(ParseBase(plainText));
    const OwnedURL withQuery(ParseBase(withQueryText));
    const OwnedURL withEmptyQuery(ParseBase(withEmptyQueryText));
    Require((plain.value.bits & 1) && (withQuery.value.bits & 1) && (withEmptyQuery.value.bits & 1));

    AdClickAttribution attribution = Make(9, &other.impl, &destination.impl);
    Require(IsNullURL(UrlForTesting(&attribution, &plain.value)));
    attribution.source = nullptr;
    Require(IsNullURL(UrlForTesting(&attribution, &plain.value)));

    attribution.source = &localhost.impl;
    Require(Throws<std::logic_error>([&] { UrlForTesting(&attribution, &plain.value); }));
    attribution.conversion = {true, {5, 1, false}};
    {
        const OwnedURL url(UrlForTesting(&attribution, &plain.value));
        Require((url.value.bits & 1) && Is(url.value.impl, "http://127.0.0.1:8000/report?conversion=5&campaign=9"));
    }
    attribution.source = &loopback.impl;
    {
        const OwnedURL url(UrlForTesting(&attribution, &withQuery.value));
        Require((url.value.bits & 1) && Is(url.value.impl, "http://127.0.0.1:8000/report?conversion=5&campaign=9&a=b&c"));
    }
    {
        const OwnedURL url(UrlForTesting(&attribution, &withEmptyQuery.value));
        Require((url.value.bits & 1) && Is(url.value.impl, "http://127.0.0.1:8000/report?conversion=5&campaign=9&"));
    }
    attribution.campaignId = 4000000000u;
    attribution.conversion.value.data = 4294967295u;
    {
        const OwnedURL url(UrlForTesting(&attribution, &plain.value));
        Require(Is(url.value.impl, "http://127.0.0.1:8000/report?conversion=4294967295&campaign=4000000000"));
    }
    Require(Throws<std::invalid_argument>([&] { UrlForTesting(nullptr, &plain.value); }));
    Require(Throws<std::invalid_argument>([&] { UrlForTesting(&attribution, nullptr); }));
}

struct Parsed {
    bool has;
    std::uint32_t data;
    std::uint32_t priority;
    bool clean;
};

Parsed ParseRequest(const URL& url) {
    ExpectedConversion result;
    std::memset(&result, 0xA5, sizeof(result));
    Require(ParseConversionRequest(&result, &url) == &result);
    if (result.has) return {true, result.value.data, result.value.priority, !result.value.wasSent};
    const unsigned char zeros[sizeof(ExpectedConversion)] = {};
    return {false, 0, 0, !std::memcmp(&result, zeros, sizeof(result))};
}

Parsed ParseText(const std::string& text) {
    Text input(text);
    const OwnedURL url(ParseBase(input));
    return ParseRequest(url.value);
}

bool Accepted(const std::string& text, std::uint32_t data, std::uint32_t priority) {
    const Parsed parsed = ParseText(text);
    return parsed.has && parsed.clean && parsed.data == data && parsed.priority == priority;
}

bool Rejected(const std::string& text) {
    const Parsed parsed = ParseText(text);
    return !parsed.has && parsed.clean;
}

void TestParseConversionRequest() {
    const std::string prefix = "https://example.com/.well-known/ad-click-attribution/";
    Require(Accepted(prefix + "12", 12, 0));
    Require(Accepted(prefix + "00", 0, 0));
    Require(Accepted(prefix + "63", 63, 0));
    Require(Accepted(prefix + "12/34", 12, 34));
    Require(Accepted(prefix + "63/63", 63, 63));
    Require(Accepted(prefix + "12x34", 12, 34));
    Require(Accepted(prefix + "+5", 5, 0));
    Require(Accepted(prefix + "+5/+6", 5, 6));
    Require(Accepted("HTTPS://EXAMPLE.com/.well-known/ad-click-attribution/07", 7, 0));
    Require(Accepted("https://example.com:8443/.well-known/ad-click-attribution/07/08", 7, 8));

    Require(Rejected(prefix + "64"));
    Require(Rejected(prefix + "99"));
    Require(Rejected(prefix + "12/64"));
    Require(Rejected(prefix + "64/12"));
    Require(Rejected(prefix + "1"));
    Require(Rejected(prefix + "123"));
    Require(Rejected(prefix + "12/3"));
    Require(Rejected(prefix + "12/345"));
    Require(Rejected(prefix + "5a"));
    Require(Rejected(prefix + "a5"));
    Require(Rejected(prefix + "-5"));
    Require(Rejected(prefix + "12/-5"));
    Require(Rejected(prefix + "12/a5"));
    Require(Rejected(prefix));
    Require(Rejected("https://example.com/"));
    Require(Rejected("https://example.com/.well-known/ad-click-attributio/12"));
    Require(Rejected("https://example.com/.Well-Known/ad-click-attribution/12"));
    Require(Rejected("https://example.com/x/.well-known/ad-click-attribution/12"));
    Require(Rejected("http://example.com/.well-known/ad-click-attribution/12"));
    Require(Rejected("httpss://example.com/.well-known/ad-click-attribution/12"));
    Require(Rejected("https://user@example.com/.well-known/ad-click-attribution/12"));
    Require(Rejected("https://user:pass@example.com/.well-known/ad-click-attribution/12"));
    Require(Rejected(prefix + "12?a"));
    Require(Rejected(prefix + "12#a"));

    const URL null{};
    const Parsed fromNull = ParseRequest(null);
    Require(!fromNull.has && fromNull.clean);

    Text asciiText(prefix + "x5");
    const OwnedURL ascii(ParseBase(asciiText));
    Require(ascii.value.bits & 1);
    Require(!ParseRequest(ascii.value).has);
    std::u16string wideUnits = Widen(prefix + "x5");
    wideUnits[wideUnits.size() - 2] = 0x2028;
    Text wideText(wideUnits);
    URL wide = ascii.value;
    wide.impl = &wideText.impl;
    const Parsed spaced = ParseRequest(wide);
    Require(spaced.has && spaced.data == 5 && spaced.priority == 0);
    wideUnits[wideUnits.size() - 2] = 0x200B;
    Text zeroWidthText(wideUnits);
    wide.impl = &zeroWidthText.impl;
    Require(!ParseRequest(wide).has);
    wideUnits[wideUnits.size() - 2] = u'5';
    wideUnits[wideUnits.size() - 1] = 0x3000;
    Text trailingText(wideUnits);
    wide.impl = &trailingText.impl;
    const Parsed trailing = ParseRequest(wide);
    Require(trailing.has && trailing.data == 5);

    ExpectedConversion result;
    Require(Throws<std::invalid_argument>([&] { ParseConversionRequest(nullptr, &null); }));
    Require(Throws<std::invalid_argument>([&] { ParseConversionRequest(&result, nullptr); }));
}

void TestToString() {
    Text source(std::string("source.example"));
    Text destination(std::string("destination.example"));
    AdClickAttribution attribution = Make(12, &source.impl, &destination.impl);
    {
        const String text = ToString(&attribution);
        Require(Is(text.impl, "Source: source.example\nDestination: destination.example\nCampaign ID: 12\nNo conversion data.\n"));
        Require(text.impl->hashAndFlags & 4);
    }
    attribution.conversion = {true, {7, 3, false}};
    {
        const String text = ToString(&attribution);
        Require(Is(text.impl, "Source: source.example\nDestination: destination.example\nCampaign ID: 12\nConversion data: 7\nConversion priority: 3\n"
                              "Conversion earliest time to send: Not set\nConversion request sent: false\n"));
    }
    const double now = _ZN3WTF8WallTime3nowEv().m_value;
    attribution.earliestTimeToSend = {true, now + 1.5 * Day};
    attribution.conversion.value.wasSent = true;
    {
        const String text = ToString(&attribution);
        Require(Is(text.impl, "Source: source.example\nDestination: destination.example\nCampaign ID: 12\nConversion data: 7\nConversion priority: 3\n"
                              "Conversion earliest time to send: Within 24-48 hours\nConversion request sent: true\n"));
    }
    for (const double offset : {0.5 * Day, 2.5 * Day, -1.0 * Day}) {
        attribution.earliestTimeToSend.value = now + offset;
        const String text = ToString(&attribution);
        Require(Units(text.impl).find(u"Conversion earliest time to send: Outside 24-48 hours\n") != std::u16string::npos);
    }

    Text wideSource(std::u16string(u"s\u00e9\u4e2d"));
    AdClickAttribution wide = Make(0, &wideSource.impl, nullptr);
    {
        const String text = ToString(&wide);
        Require(Units(text.impl) == u"Source: s\u00e9\u4e2d\nDestination: \nCampaign ID: 0\nNo conversion data.\n");
        Require(!(text.impl->hashAndFlags & 4));
    }
    AdClickAttribution empty{};
    {
        const String text = ToString(&empty);
        Require(Is(text.impl, "Source: \nDestination: \nCampaign ID: 0\nNo conversion data.\n"));
    }
    Require(Throws<std::invalid_argument>([] { ToString(nullptr); }));
}

template<typename TAction> bool ThrowsNotImplemented(TAction action) {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    try {
        action();
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::logic_error&) {
        return false;
    } catch (const std::runtime_error&) {
        return NotImplementedCalls_nid_no_patch() == before + 1;
    } catch (...) {
        return false;
    }
    return false;
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* Writable() const { return base; }
    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }

    template<typename T>
    T* SplitReadWriteToReadOnly() const {
        return reinterpret_cast<T*>(base + pageSize - sizeof(T) / 2);
    }

    template<typename T>
    T* SplitReadOnlyToGuard() const {
        return reinterpret_cast<T*>(base + pageSize * 2 - sizeof(T) / 2);
    }
};

void TestGuestPointers() {
    GuardedPages pages;
    const auto* unmappedUrl = reinterpret_cast<const URL*>(pages.Guard());
    const auto* splitUrl = pages.SplitReadOnlyToGuard<const URL>();
    auto* unmappedExpected = reinterpret_cast<ExpectedConversion*>(pages.Guard());
    auto* readOnlyExpected = reinterpret_cast<ExpectedConversion*>(pages.ReadOnly());
    auto* splitExpected = pages.SplitReadWriteToReadOnly<ExpectedConversion>();

    Text validUrlText(std::string("https://example.com/.well-known/ad-click-attribution/12/34"));
    const OwnedURL validUrl(ParseBase(validUrlText));
    Require((validUrl.value.bits & 1) != 0);

    ExpectedConversion sentinel{};
    std::memset(&sentinel, 0x5A, sizeof(sentinel));
    Require(ThrowsNotImplemented([&] { ParseConversionRequest(&sentinel, unmappedUrl); }));
    for (std::size_t i = 0; i < sizeof(sentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&sentinel)[i] == 0x5A);
    Require(ThrowsNotImplemented([&] { ParseConversionRequest(&sentinel, splitUrl); }));
    for (std::size_t i = 0; i < sizeof(sentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&sentinel)[i] == 0x5A);
    Require(ThrowsNotImplemented([&] { ParseConversionRequest(unmappedExpected, &validUrl.value); }));
    Require(ThrowsNotImplemented([&] { ParseConversionRequest(readOnlyExpected, &validUrl.value); }));

    const std::size_t splitExpectedHead = sizeof(ExpectedConversion) / 2;
    std::memset(splitExpected, 0x6B, splitExpectedHead);
    Require(ThrowsNotImplemented([&] { ParseConversionRequest(splitExpected, &validUrl.value); }));
    for (std::size_t i = 0; i < splitExpectedHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitExpected)[i] == 0x6B);

    URL badImplUrl = validUrl.value;
    badImplUrl.impl = reinterpret_cast<StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { ParseConversionRequest(&sentinel, &badImplUrl); }));
    for (std::size_t i = 0; i < sizeof(sentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&sentinel)[i] == 0x5A);

    StringImpl badDataImpl{2, 64, pages.Guard(), 4};
    URL badDataUrl = validUrl.value;
    badDataUrl.impl = &badDataImpl;
    Require(ThrowsNotImplemented([&] { ParseConversionRequest(&sentinel, &badDataUrl); }));
    for (std::size_t i = 0; i < sizeof(sentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&sentinel)[i] == 0x5A);

    StringImpl splitDataImpl{2, 16, pages.Guard() - 4, 4};
    URL splitDataUrl = validUrl.value;
    splitDataUrl.impl = &splitDataImpl;
    Require(ThrowsNotImplemented([&] { ParseConversionRequest(&sentinel, &splitDataUrl); }));
    for (std::size_t i = 0; i < sizeof(sentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&sentinel)[i] == 0x5A);

    Text source(std::string("source.example"));
    Text destination(std::string("destination.example"));
    Text localhost(std::string("localhost"));
    AdClickAttribution attribution = Make(12, &source.impl, &destination.impl);
    const AdClickAttribution originalAttribution = attribution;
    OptionalSeconds optSentinel{};
    std::memset(&optSentinel, 0x4C, sizeof(optSentinel));
    const Conversion validConversion{7, 3, false};

    auto* unmappedAttr = reinterpret_cast<AdClickAttribution*>(pages.Guard());
    auto* readOnlyAttr = reinterpret_cast<AdClickAttribution*>(pages.ReadOnly());
    auto* splitAttrRwRo = pages.SplitReadWriteToReadOnly<AdClickAttribution>();
    const auto* splitAttrRoGuard = pages.SplitReadOnlyToGuard<const AdClickAttribution>();
    auto* unmappedOpt = reinterpret_cast<OptionalSeconds*>(pages.Guard());
    auto* readOnlyOpt = reinterpret_cast<OptionalSeconds*>(pages.ReadOnly());
    auto* splitOpt = pages.SplitReadWriteToReadOnly<OptionalSeconds>();
    const auto* unmappedConv = reinterpret_cast<const Conversion*>(pages.Guard());
    const auto* splitConv = pages.SplitReadOnlyToGuard<const Conversion>();

    Require(ThrowsNotImplemented([&] { Convert(unmappedOpt, &attribution, &validConversion); }));
    Require(std::memcmp(&attribution, &originalAttribution, sizeof(attribution)) == 0);
    Require(ThrowsNotImplemented([&] { Convert(readOnlyOpt, &attribution, &validConversion); }));
    Require(std::memcmp(&attribution, &originalAttribution, sizeof(attribution)) == 0);
    const std::size_t splitOptHead = sizeof(OptionalSeconds) / 2;
    std::memset(splitOpt, 0x7D, splitOptHead);
    Require(ThrowsNotImplemented([&] { Convert(splitOpt, &attribution, &validConversion); }));
    for (std::size_t i = 0; i < splitOptHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitOpt)[i] == 0x7D);
    Require(std::memcmp(&attribution, &originalAttribution, sizeof(attribution)) == 0);

    Require(ThrowsNotImplemented([&] { Convert(&optSentinel, unmappedAttr, &validConversion); }));
    for (std::size_t i = 0; i < sizeof(optSentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&optSentinel)[i] == 0x4C);
    Require(ThrowsNotImplemented([&] { Convert(&optSentinel, readOnlyAttr, &validConversion); }));
    for (std::size_t i = 0; i < sizeof(optSentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&optSentinel)[i] == 0x4C);
    const std::size_t splitAttrHead = sizeof(AdClickAttribution) / 2;
    std::memset(splitAttrRwRo, 0x3E, splitAttrHead);
    Require(ThrowsNotImplemented([&] { Convert(&optSentinel, splitAttrRwRo, &validConversion); }));
    for (std::size_t i = 0; i < splitAttrHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitAttrRwRo)[i] == 0x3E);
    for (std::size_t i = 0; i < sizeof(optSentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&optSentinel)[i] == 0x4C);

    Require(ThrowsNotImplemented([&] { Convert(&optSentinel, &attribution, unmappedConv); }));
    Require(ThrowsNotImplemented([&] { Convert(&optSentinel, &attribution, splitConv); }));
    for (std::size_t i = 0; i < sizeof(optSentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&optSentinel)[i] == 0x4C);
    Require(std::memcmp(&attribution, &originalAttribution, sizeof(attribution)) == 0);

    AdClickAttribution aliased = Make(5, &source.impl, &destination.impl);
    aliased.conversion = {false, {11, 9, true}};
    OptionalSeconds aliasedResult{};
    Require(Convert(&aliasedResult, &aliased, &aliased.conversion.value) == &aliasedResult);
    Require(aliasedResult.engaged && aliased.conversion.engaged && aliased.conversion.value.data == 11 && aliased.conversion.value.priority == 9 && aliased.conversion.value.wasSent);

    Require(ThrowsNotImplemented([&] { HasHigherPriorityThan(unmappedAttr, &attribution); }));
    Require(ThrowsNotImplemented([&] { HasHigherPriorityThan(splitAttrRoGuard, &attribution); }));
    Require(ThrowsNotImplemented([&] { HasHigherPriorityThan(&attribution, unmappedAttr); }));
    Require(ThrowsNotImplemented([&] { HasHigherPriorityThan(&attribution, splitAttrRoGuard); }));

    Require(ThrowsNotImplemented([&] { Url(unmappedAttr); }));
    Require(ThrowsNotImplemented([&] { Url(splitAttrRoGuard); }));
    Require(ThrowsNotImplemented([&] { Referrer(unmappedAttr); }));
    Require(ThrowsNotImplemented([&] { Referrer(splitAttrRoGuard); }));

    AdClickAttribution ready = Make(12, &source.impl, &destination.impl);
    ready.conversion = {true, {7, 3, false}};
    ready.earliestTimeToSend = {true, 1.0};
    AdClickAttribution badSource = ready;
    badSource.source = reinterpret_cast<StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { Url(&badSource); }));
    Require(ThrowsNotImplemented([&] { Referrer(&badSource); }));
    Require(ThrowsNotImplemented([&] { ToString(&badSource); }));
    badSource.source = &badDataImpl;
    Require(ThrowsNotImplemented([&] { Url(&badSource); }));
    Require(ThrowsNotImplemented([&] { ToString(&badSource); }));
    badSource.source = &splitDataImpl;
    Require(ThrowsNotImplemented([&] { Url(&badSource); }));

    AdClickAttribution badDest = ready;
    badDest.destination = reinterpret_cast<StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { Url(&badDest); }));
    Require(ThrowsNotImplemented([&] { Referrer(&badDest); }));
    Require(ThrowsNotImplemented([&] { ToString(&badDest); }));
    badDest.destination = &badDataImpl;
    Require(ThrowsNotImplemented([&] { Referrer(&badDest); }));
    Require(ThrowsNotImplemented([&] { ToString(&badDest); }));

    Text baseText(std::string("http://127.0.0.1:8000/report?a=b"));
    const OwnedURL baseUrl(ParseBase(baseText));
    AdClickAttribution local = Make(9, &localhost.impl, &destination.impl);
    local.conversion = {true, {5, 1, false}};
    Require(ThrowsNotImplemented([&] { UrlForTesting(unmappedAttr, &baseUrl.value); }));
    Require(ThrowsNotImplemented([&] { UrlForTesting(splitAttrRoGuard, &baseUrl.value); }));
    Require(ThrowsNotImplemented([&] { UrlForTesting(&local, unmappedUrl); }));
    Require(ThrowsNotImplemented([&] { UrlForTesting(&local, splitUrl); }));
    URL badBaseImpl = baseUrl.value;
    badBaseImpl.impl = reinterpret_cast<StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { UrlForTesting(&local, &badBaseImpl); }));
    URL badBaseData = baseUrl.value;
    badBaseData.impl = &badDataImpl;
    Require(ThrowsNotImplemented([&] { UrlForTesting(&local, &badBaseData); }));
    URL splitBaseData = baseUrl.value;
    splitBaseData.impl = &splitDataImpl;
    Require(ThrowsNotImplemented([&] { UrlForTesting(&local, &splitBaseData); }));
    AdClickAttribution localBadSource = local;
    localBadSource.source = reinterpret_cast<StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { UrlForTesting(&localBadSource, &baseUrl.value); }));

    Require(ThrowsNotImplemented([&] { MarkAsExpired(unmappedAttr); }));
    Require(ThrowsNotImplemented([&] { MarkAsExpired(readOnlyAttr); }));
    std::memset(splitAttrRwRo, 0x2A, splitAttrHead);
    Require(ThrowsNotImplemented([&] { MarkAsExpired(splitAttrRwRo); }));
    for (std::size_t i = 0; i < splitAttrHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitAttrRwRo)[i] == 0x2A);

    Require(ThrowsNotImplemented([&] { HasExpired(unmappedAttr); }));
    Require(ThrowsNotImplemented([&] { HasExpired(splitAttrRoGuard); }));

    Require(ThrowsNotImplemented([&] { MarkConversionAsSent(unmappedAttr); }));
    Require(ThrowsNotImplemented([&] { MarkConversionAsSent(readOnlyAttr); }));
    std::memset(splitAttrRwRo, 0x1B, splitAttrHead);
    Require(ThrowsNotImplemented([&] { MarkConversionAsSent(splitAttrRwRo); }));
    for (std::size_t i = 0; i < splitAttrHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitAttrRwRo)[i] == 0x1B);

    Require(ThrowsNotImplemented([&] { WasConversionSent(unmappedAttr); }));
    Require(ThrowsNotImplemented([&] { WasConversionSent(splitAttrRoGuard); }));

    Require(ThrowsNotImplemented([&] { ToString(unmappedAttr); }));
    Require(ThrowsNotImplemented([&] { ToString(splitAttrRoGuard); }));
}

}

int main() {
    TestConversion();
    TestPriority();
    TestExpiry();
    TestUrls();
    TestUrlForTesting();
    TestParseConversionRequest();
    TestToString();
    TestGuestPointers();
    return 0;
}
