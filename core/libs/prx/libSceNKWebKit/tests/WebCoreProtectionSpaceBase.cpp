#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore::ProtectionSpaceBase check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

using Predicate = bool (APS5_VABI *)(const WebCore::ProtectionSpaceBase*);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore19ProtectionSpaceBase15isPasswordBasedEv), Predicate>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore19ProtectionSpaceBase26receivesCredentialSecurelyEv), Predicate>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore19ProtectionSpaceBase7isProxyEv), Predicate>);
static_assert(sizeof(WebCore::ProtectionSpaceBase) == 24);
static_assert(offsetof(WebCore::ProtectionSpaceBase, m_serverType) == 20);
static_assert(offsetof(WebCore::ProtectionSpaceBase, m_authenticationScheme) == 21);

WebCore::ProtectionSpaceBase Make(std::uint8_t serverType, std::uint8_t authenticationScheme) {
    return WebCore::ProtectionSpaceBase{nullptr, nullptr, 80, serverType, authenticationScheme, false};
}

void TestOffsets() {
    alignas(8) unsigned char raw[24] = {};
    const auto* space = reinterpret_cast<const WebCore::ProtectionSpaceBase*>(raw);

    raw[20] = 5;
    raw[21] = 1;
    Require(_ZNK7WebCore19ProtectionSpaceBase7isProxyEv(space));
    Require(!_ZNK7WebCore19ProtectionSpaceBase26receivesCredentialSecurelyEv(space));

    raw[20] = 2;
    raw[21] = 2;
    Require(!_ZNK7WebCore19ProtectionSpaceBase7isProxyEv(space));
    Require(_ZNK7WebCore19ProtectionSpaceBase26receivesCredentialSecurelyEv(space));
    Require(_ZNK7WebCore19ProtectionSpaceBase15isPasswordBasedEv(space));

    raw[21] = 7;
    Require(!_ZNK7WebCore19ProtectionSpaceBase15isPasswordBasedEv(space));
    std::printf("ProtectionSpaceBase offsets: server type at byte 20, scheme at byte 21\n");
}

void TestIsProxy() {
    const bool expected[8] = {false, false, false, false, true, true, true, true};
    for (std::uint8_t serverType = 1; serverType <= 8; ++serverType) {
        const WebCore::ProtectionSpaceBase space = Make(serverType, 1);
        Require(_ZNK7WebCore19ProtectionSpaceBase7isProxyEv(&space) == expected[serverType - 1]);
    }

    const std::uint8_t outside[] = {0, 9, 255};
    for (std::uint8_t serverType : outside) {
        const WebCore::ProtectionSpaceBase space = Make(serverType, 1);
        Require(!_ZNK7WebCore19ProtectionSpaceBase7isProxyEv(&space));
    }
    std::printf("ProtectionSpaceBase::isProxy: server types 1 to 8, 0, 9 and 255 passed\n");
}

void TestReceivesCredentialSecurely() {
    const bool basic[8] = {false, true, false, true, false, true, false, false};
    for (std::uint8_t serverType = 1; serverType <= 8; ++serverType) {
        const WebCore::ProtectionSpaceBase space = Make(serverType, 2);
        Require(_ZNK7WebCore19ProtectionSpaceBase26receivesCredentialSecurelyEv(&space) == basic[serverType - 1]);
    }

    for (std::uint8_t serverType = 1; serverType <= 8; ++serverType) {
        const WebCore::ProtectionSpaceBase space = Make(serverType, 3);
        Require(_ZNK7WebCore19ProtectionSpaceBase26receivesCredentialSecurelyEv(&space));
    }
    std::printf("ProtectionSpaceBase::receivesCredentialSecurely: HTTP basic and digest passed\n");
}

void TestIsPasswordBased() {
    for (unsigned scheme = 0; scheme <= 255; ++scheme) {
        const bool expected = scheme != 7 && scheme != 8 && scheme != 100;
        const WebCore::ProtectionSpaceBase space = Make(1, static_cast<std::uint8_t>(scheme));
        Require(_ZNK7WebCore19ProtectionSpaceBase15isPasswordBasedEv(&space) == expected);
    }
    std::printf("ProtectionSpaceBase::isPasswordBased: every scheme byte 0 to 255 passed\n");
}

void TestFieldIndependence() {
    NKWebKit::WtfAbi::StringImpl host{2, 0, nullptr, 4};
    NKWebKit::WtfAbi::StringImpl realm{2, 0, nullptr, 4};
    const std::uint8_t serverTypes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 255};
    const std::uint8_t schemes[] = {0, 1, 2, 3, 7, 8, 9, 100, 255};

    for (std::uint8_t serverType : serverTypes) {
        for (std::uint8_t scheme : schemes) {
            const WebCore::ProtectionSpaceBase plain = Make(serverType, scheme);
            WebCore::ProtectionSpaceBase changed = plain;
            changed.m_host = &host;
            changed.m_realm = &realm;
            changed.m_port = 443;
            changed.m_isHashTableDeletedValue = true;
            Require(_ZNK7WebCore19ProtectionSpaceBase7isProxyEv(&plain) == _ZNK7WebCore19ProtectionSpaceBase7isProxyEv(&changed));
            Require(_ZNK7WebCore19ProtectionSpaceBase26receivesCredentialSecurelyEv(&plain) == _ZNK7WebCore19ProtectionSpaceBase26receivesCredentialSecurelyEv(&changed));
            Require(_ZNK7WebCore19ProtectionSpaceBase15isPasswordBasedEv(&plain) == _ZNK7WebCore19ProtectionSpaceBase15isPasswordBasedEv(&changed));
        }
    }
    std::printf("ProtectionSpaceBase predicates: host, realm, port and deleted-value flag do not affect results\n");
}

void TestNullThrows() {
    RequireThrows([] { _ZNK7WebCore19ProtectionSpaceBase7isProxyEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore19ProtectionSpaceBase26receivesCredentialSecurelyEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore19ProtectionSpaceBase15isPasswordBasedEv(nullptr); });
    std::printf("ProtectionSpaceBase predicates: null self throws\n");
}

}

int main() {
    TestOffsets();
    TestIsProxy();
    TestReceivesCredentialSecurely();
    TestIsPasswordBased();
    TestFieldIndependence();
    TestNullThrows();
}
