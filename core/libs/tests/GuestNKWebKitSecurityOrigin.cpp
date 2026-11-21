#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition) {
    if (condition) return;
    std::puts("NKWebKit security origin check failed");
    std::abort();
}

void CheckPort(int port, std::uint16_t expected) {
    auto protocol = NKWebKit::String::FromUtf8("https");
    auto host = NKWebKit::String::FromUtf8("example.org");
    const void* origin = WKSecurityOriginCreate(protocol.Get(), host.Get(), port);
    Require(origin != nullptr);
    Require(WKSecurityOriginGetPort(origin) == expected);
    WKRelease(origin);
}

}

int main() {
    CheckPort(-1, 0);
    CheckPort(-65535, 0);
    CheckPort(0, 0);
    CheckPort(443, 443);
    CheckPort(65535, 65535);
    CheckPort(65536, 0);
    std::puts("NKWebKit security origin tests passed");
    return 0;
}
