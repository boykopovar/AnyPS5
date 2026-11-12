#include "prx/libSceNKWebKit/include/UrlHelpers.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

using NKWebKit::UrlAbi::StringView;
using NKWebKit::WtfAbi::String;

constexpr auto SerializeForm = _ZN3WTF9URLParser9serializeERKNS_6VectorINS_12KeyValuePairINS_6StringES3_EELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE;

void Check(bool result, int line) {
    if (result) return;
    std::fprintf(stderr, "WTF URL helper check failed at line %d\n", line);
    std::abort();
}

#define Require(condition) Check((condition), __LINE__)

StringView View(const std::string& value) {
    return {value.data(), static_cast<std::uint32_t>(value.size()), true};
}

StringView View(const std::u16string& value) {
    return {value.data(), static_cast<std::uint32_t>(value.size()), false};
}

String MakeString(std::string_view value) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(value.data()), static_cast<unsigned>(value.size()));
}

String MakeString(std::u16string_view value) {
    return _ZN3WTF10StringImpl6createEPKDsj(value.data(), static_cast<unsigned>(value.size()));
}

std::u16string Contents(const String& value) {
    Require(value.impl);
    if (value.impl->hashAndFlags & 4) {
        const auto* bytes = static_cast<const unsigned char*>(value.impl->data);
        return {bytes, bytes + value.impl->length};
    }
    return {static_cast<const char16_t*>(value.impl->data), value.impl->length};
}

bool Uuid(const std::string& value) {
    return _ZN3WTF14isVersion4UUIDENS_10StringViewE(View(value));
}

void TestUuid() {
    const std::string valid = "123e4567-e89b-42d3-a456-426614174000";
    Require(Uuid(valid));
    Require(Uuid("123E4567-E89B-42D3-B456-426614174000"));
    Require(Uuid("123e4567-e89b-42d3-8456-426614174000"));
    Require(Uuid("123e4567-e89b-42d3-9456-426614174000"));
    Require(Uuid(" 23e4567-e89b-42d3-a456-426614174000"));
    Require(Uuid("123e4567-e89b-42d3-a456-42661417400 "));
    Require(!Uuid(""));
    Require(!Uuid("123e4567-e89b-42d3-a456-42661417400"));
    Require(!Uuid("123e4567-e89b-42d3-a456-4266141740000"));
    Require(!Uuid("123e4567-e89b-12d3-a456-426614174000"));
    Require(!Uuid("123e4567-e89b-52d3-a456-426614174000"));
    Require(!Uuid("123e4567-e89b-42d3-7456-426614174000"));
    Require(!Uuid("123e4567-e89b-42d3-c456-426614174000"));
    Require(!Uuid("123e4567-e89b-42d3-a456-42661417400g"));
    Require(!Uuid("123e4567+e89b-42d3-a456-426614174000"));
    Require(!Uuid("123e4567-+89b-42d3-a456-426614174000"));
    Require(!Uuid("123e4567-e89b-42d3-a456-+26614174000"));
    Require(!Uuid("123e4567-e89b-42d3-a456-4266141740-0"));
    auto embeddedNull = valid;
    embeddedNull[5] = '\0';
    Require(!Uuid(embeddedNull));
    Require(_ZN3WTF14isVersion4UUIDENS_10StringViewE(View(std::u16string(valid.begin(), valid.end()))));
    auto wide = std::u16string(valid.begin(), valid.end());
    wide[0] = u'\u20AC';
    Require(!_ZN3WTF14isVersion4UUIDENS_10StringViewE(View(wide)));
    Require(!_ZN3WTF14isVersion4UUIDENS_10StringViewE({nullptr, 0, true}));
    Require(!_ZN3WTF14isVersion4UUIDENS_10StringViewE({nullptr, 0, false}));
    Require(valid == "123e4567-e89b-42d3-a456-426614174000");
}

bool Port(std::uint16_t port, const std::string& protocol) {
    return _ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(port, View(protocol));
}

void TestPorts() {
    Require(Port(21, "ftp"));
    Require(Port(80, "http"));
    Require(Port(80, "ws"));
    Require(Port(443, "https"));
    Require(Port(443, "wss"));
    Require(!Port(0, "http"));
    Require(!Port(443, "http"));
    Require(!Port(80, "https"));
    Require(!Port(65535, "wss"));
    Require(!Port(21, "FTp"));
    Require(!Port(80, "HTTP"));
    Require(!Port(80, "htt"));
    Require(!Port(80, "http:"));
    Require(!Port(21, "file"));
    Require(!Port(22, "ssh"));
    Require(!Port(0, ""));
    Require(!Port(80, std::string("http\0extra", 10)));
    Require(_ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(443, View(std::u16string(u"https"))));
    Require(!_ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(443, View(std::u16string(u"HTTPS"))));
    Require(!_ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(443, View(std::u16string(u"h\u00E9tps"))));
    Require(!_ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(0, {nullptr, 0, true}));
    Require(!_ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(80, {nullptr, 0, false}));
}

void TestPortOverrides() {
    _ZN3WTF40clearDefaultPortForProtocolMapForTestingEv();
    _ZN3WTF40registerDefaultPortForProtocolForTestingEtRKNS_6StringE(8080, MakeString(u"http"));
    _ZN3WTF40registerDefaultPortForProtocolForTestingEtRKNS_6StringE(1234, MakeString("http"));
    _ZN3WTF40registerDefaultPortForProtocolForTestingEtRKNS_6StringE(0, MakeString("custom"));
    Require(Port(8080, "http"));
    Require(!Port(1234, "http"));
    Require(!Port(80, "http"));
    Require(!Port(8080, "HTTP"));
    Require(Port(0, "custom"));
    Require(Port(443, "https"));
    _ZN3WTF40clearDefaultPortForProtocolMapForTestingEv();
    Require(Port(80, "http"));
    Require(!Port(8080, "http"));
    Require(!Port(0, "custom"));
}

void TestSchemes() {
    const auto lower = MakeString("https");
    const auto unchanged = _ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(lower);
    Require(unchanged.hasValue && unchanged.value.impl == lower.impl);
    const auto ascii = _ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(MakeString("HttPS"));
    Require(ascii.hasValue && Contents(ascii.value) == u"https");
    const auto wide = _ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(MakeString(u"My+SCHEME-1.2"));
    Require(wide.hasValue && Contents(wide.value) == u"my+scheme-1.2");
    Require(!_ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(String()).hasValue);
    for (const auto invalid : {"", "1http", "https:", "ht\ttps", "http ", "a_b"}) {
        Require(!_ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(MakeString(invalid)).hasValue);
    }
    Require(!_ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(MakeString(u"a\u00E9")).hasValue);
    Require(!_ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(MakeString(std::string_view("a\0b", 3))).hasValue);
}

void TestForms() {
    const auto empty = _ZN3WTF9URLParser19parseURLEncodedFormENS_10StringViewE({nullptr, 0, true});
    Require(!empty.size && !empty.buffer);
    Require(!SerializeForm(empty).impl);

    const auto fields = _ZN3WTF9URLParser19parseURLEncodedFormENS_10StringViewE(
        View(std::string("&&a+b=%2b+%C3%A9&a+b=two=2&bare&=v&=&bad=%E2%82x%ED%A0%80%ZZ%00&&")));
    Require(fields.size == 6);
    Require(Contents(fields[0].key) == u"a b" && Contents(fields[0].value) == u"+ \u00E9");
    Require(Contents(fields[1].key) == u"a b" && Contents(fields[1].value) == u"two=2");
    Require(Contents(fields[2].key) == u"bare" && Contents(fields[2].value).empty());
    Require(Contents(fields[3].key).empty() && Contents(fields[3].value) == u"v");
    Require(Contents(fields[4].key).empty() && Contents(fields[4].value).empty());
    Require(Contents(fields[5].value) == std::u16string(u"\uFFFDx\uFFFD\uFFFD\uFFFD%ZZ\0", 9));
    const auto encoded = SerializeForm(fields);
    Require(Contents(encoded) == u"a+b=%2B+%C3%A9&a+b=two%3D2&bare=&=v&=&bad=%EF%BF%BDx%EF%BF%BD%EF%BF%BD%EF%BF%BD%25ZZ%00");

    std::u16string wide = u"\U0001F600=*_.-~&invalid=";
    wide.push_back(0xD800);
    wide += u"&last=%26%3D";
    const auto wideFields = _ZN3WTF9URLParser19parseURLEncodedFormENS_10StringViewE(View(wide));
    Require(wideFields.size == 2);
    Require(Contents(wideFields[0].key) == u"\U0001F600" && Contents(wideFields[1].value) == u"&=");
    const auto wideEncoded = SerializeForm(wideFields);
    Require(Contents(wideEncoded) == u"%F0%9F%98%80=*_.-%7E&last=%26%3D");
}

}

int main() {
    TestUuid();
    TestPorts();
    TestPortOverrides();
    TestSchemes();
    TestForms();
    return 0;
}
