#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Connection check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    auto conn = NKWebKit::Make<NKWebKit::Connection>();
    const void* connRef = NKWebKit::ToRef(conn);

    auto name = NKWebKit::String::FromUtf8("Ping");
    const void* nameRef = NKWebKit::ToRef(name);
    auto body = NKWebKit::String::FromUtf8("Payload");
    const void* bodyRef = NKWebKit::ToRef(body);

    WKConnectionPostMessage(connRef, nameRef, bodyRef);
    Require(conn->lastMessageName.Get() != nullptr);
    Require(conn->lastMessageBody.Get() != nullptr);

    int dummyClient = 1;
    WKConnectionSetConnectionClient(connRef, &dummyClient);
    Require(conn->clientConfigured);
    WKConnectionSetConnectionClient(connRef, nullptr);
    Require(!conn->clientConfigured);

    RequireInvalidArgument([&] { WKConnectionPostMessage(nullptr, nameRef, bodyRef); });
    RequireInvalidArgument([&] { WKConnectionSetConnectionClient(nullptr, &dummyClient); });

    WKRelease(bodyRef);
    WKRelease(nameRef);
    WKRelease(connRef);
    return 0;
}
