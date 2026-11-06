#include "prx/libSceNKWebKit/include/UrlHelpers.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

using NKWebKit::UrlAbi::StringView;

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

}

int main() {
    TestUuid();
    TestPorts();
    return 0;
}
