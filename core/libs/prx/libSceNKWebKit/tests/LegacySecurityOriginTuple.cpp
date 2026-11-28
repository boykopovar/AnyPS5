#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using NKWebKit::WebCoreAbi::LegacyOptionalSecurityOriginTuple;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "legacy security origin tuple check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

constexpr auto fromDatabaseIdentifier = &_ZN7WebCore18SecurityOriginData22fromDatabaseIdentifierERKN3WTF6StringE;

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Text(const std::string& text) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(text.c_str()));
}

static std::u16string Units(const StringImpl* impl) {
    const StringView view = NKWebKit::WtfAbi::ViewOf(impl);
    std::u16string text;
    for (unsigned index = 0; index < view.length; ++index) text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    return text;
}

static StringImpl* Empty() {
    static StringImpl* const impl = NKWebKit::WtfAbi::EmptyStringImpl();
    return impl;
}

static bool Is8Bit(const StringImpl* impl) {
    return (impl->hashAndFlags & 4) != 0;
}

struct Parsed {
    alignas(8) unsigned char bytes[32];

    explicit Parsed(const String& identifier) {
        std::memset(bytes, 0xA5, sizeof(bytes));
        Require(fromDatabaseIdentifier(get(), &identifier) == get());
        Require(bytes[0] == 0 || bytes[0] == 1);
    }

    ~Parsed() {
        if (!get()->engaged) return;
        NKWebKit::WtfAbi::Release(get()->value.protocol);
        NKWebKit::WtfAbi::Release(get()->value.host);
    }

    LegacyOptionalSecurityOriginTuple* get() {
        return reinterpret_cast<LegacyOptionalSecurityOriginTuple*>(bytes);
    }
};

static void RequireOrigin(const std::string& identifier, const std::u16string& protocol, const std::u16string& host, int port, int line) {
    const String text = Text(identifier);
    const unsigned before = text.impl->refCount;
    const unsigned emptyBefore = Empty()->refCount;
    Parsed parsed(text);
    const LegacyOptionalSecurityOriginTuple& result = *parsed.get();
    Check(result.engaged, line);
    Check(result.value.protocol && result.value.host, line);
    Check(Units(result.value.protocol) == protocol && Units(result.value.host) == host, line);
    Check(result.value.port.engaged == (port != 0), line);
    if (port) Check(result.value.port.value == port, line);
    Check(text.impl->refCount == before, line);
    const unsigned emptyParts = (protocol.empty() ? 1U : 0U) + (host.empty() ? 1U : 0U);
    Check(Empty()->refCount == emptyBefore + 2 * emptyParts, line);
    for (const StringImpl* impl : {result.value.protocol, result.value.host}) {
        if (NKWebKit::WtfAbi::ViewOf(impl).length) {
            Check(impl->refCount == 2 && impl != text.impl, line);
            Check(Is8Bit(impl) == Is8Bit(text.impl), line);
        } else {
            Check(impl == Empty(), line);
        }
    }
}

static void RequireNothing(const std::string& identifier, int line) {
    const String text = Text(identifier);
    Parsed parsed(text);
    Check(!parsed.get()->engaged, line);
    for (std::size_t index = 1; index < sizeof(parsed.bytes); ++index) Check(parsed.bytes[index] == 0xA5, line);
}

#define Origin(identifier, protocol, host, port) RequireOrigin((identifier), (protocol), (host), (port), __LINE__)
#define Nothing(identifier) RequireNothing((identifier), __LINE__)

static void TestTheSeparators() {
    Origin("http_example.com_80", u"http", u"example.com", 80);
    Origin("https_example.com_0", u"https", u"example.com", 0);
    Origin("https_example.com_", u"https", u"example.com", 0);
    Origin("file__0", u"file", u"", 0);
    Origin("_host_8080", u"", u"host", 8080);
    Origin("__", u"", u"", 0);
    Origin("___", u"", u"_", 0);
    Origin("http_my_intranet_host_443", u"http", u"my_intranet_host", 443);
    Origin("http_host%5Fname_1", u"http", u"host%5Fname", 1);
    Nothing("_");
    Nothing("");
    Nothing("nounderscore");
    Nothing("http_example.com");
    Nothing("example.com_80");
    {
        const String null;
        Parsed parsed(null);
        Require(!parsed.get()->engaged);
    }
}

static void TestThePort() {
    Origin("http_host_65535", u"http", u"host", 65535);
    Origin("http_host_0080", u"http", u"host", 80);
    Origin("http_host_00", u"http", u"host", 0);
    Origin("http_host_+80", u"http", u"host", 80);
    Origin("http_host_-0", u"http", u"host", 0);
    Origin("http_host_ 80", u"http", u"host", 80);
    Origin("http_host_\t\n\r 80", u"http", u"host", 80);
    Origin("http_host_80abc", u"http", u"host", 80);
    Origin("http_host_80 ", u"http", u"host", 80);
    Origin("http_host_80+1", u"http", u"host", 80);
    Origin("http_host_8 0", u"http", u"host", 8);
    Nothing("http_host_65536");
    Nothing("http_host_-1");
    Nothing("http_host_abc");
    Nothing("http_host_ ");
    Nothing("http_host_+");
    Nothing("http_host_-");
    Nothing("http_host_+-80");
    Nothing("http_host_ +");
    Nothing("http_host_x80");
    Nothing("http_host_2147483647");
    Nothing("http_host_2147483648");
    Nothing("http_host_4294967376");
    Nothing("http_host_42949672960080");
    Nothing("http_host_-2147483648");
    Nothing("http_host_-4294967216");
    Nothing("http_host_99999999999999999999");
}

static void TestWideStrings() {
    Origin("http_\xe4\xb8\xad\xe6\x96\x87_8080", u"http", u"中文", 8080);
    Origin("\xe4\xb8\xad_host_", u"中", u"host", 0);
    Origin("http_host_\xe2\x80\x83""80\xe4\xb8\xad", u"http", u"host", 80);
    Origin("http_host_\xe3\x80\x80+443\xe4\xb8\xad", u"http", u"host", 443);
    Nothing("http_host_\xe4\xb8\xad""80");
    Nothing("http_\xe4\xb8\xad");
    const String wide = Text("http_host_\xe2\x80\x83""80\xe4\xb8\xad");
    Require(!Is8Bit(wide.impl));
    const String narrow = Text("http_host_\xc2\xa0""80");
    Parsed parsed(narrow);
    Require(!parsed.get()->engaged);
}

static void TestInvalidArguments() {
    const String text = Text("http_host_80");
    LegacyOptionalSecurityOriginTuple parsed{};
    Require(ThrowsInvalidArgument([&] { fromDatabaseIdentifier(nullptr, &text); }));
    Require(ThrowsInvalidArgument([&] { fromDatabaseIdentifier(&parsed, nullptr); }));
    Require(!parsed.engaged && text.impl->refCount == 2);
}

int main() {
    TestTheSeparators();
    TestThePort();
    TestWideStrings();
    TestInvalidArguments();
    std::puts("libSceNKWebKit legacy security origin tuple tests passed");
    return 0;
}
