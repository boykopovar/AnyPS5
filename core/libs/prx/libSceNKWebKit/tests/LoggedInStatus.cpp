#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using NKWebKit::WebCoreAbi::LoggedInStatus;
using NKWebKit::WebCoreAbi::LoggedInStatusOrException;
using NKWebKit::WebCoreAbi::RegistrableDomain;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "LoggedInStatus check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr double Week = 24.0 * 60 * 60 * 7;
constexpr double NinetyDays = 24.0 * 60 * 60 * 90;
constexpr std::uint8_t WebAuthn = 0;
constexpr std::uint8_t PasswordManager = 1;
constexpr std::uint8_t Unmanaged = 2;

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

static String Utf16(const char16_t* text, unsigned length) {
    return _ZN3WTF10StringImpl6createEPKDsj(text, length);
}

static bool Is(const String& string, const char* text) {
    return string.impl && string.impl->length == std::strlen(text) && (string.impl->hashAndFlags & 4) && std::memcmp(string.impl->data, text, string.impl->length) == 0;
}

static double Now() {
    return _ZN3WTF8WallTime3nowEv().m_value;
}

static LoggedInStatusOrException Create(const RegistrableDomain& domain, const String& username, std::uint8_t authType) {
    return _ZN7WebCore14LoggedInStatus6createERKNS_17RegistrableDomainERKN3WTF6StringENS0_19CredentialTokenTypeENS0_18AuthenticationTypeE(&domain, &username, true, authType);
}

static LoggedInStatusOrException Create(const RegistrableDomain& domain, const String& username, std::uint8_t authType, double timeToLive) {
    return _ZN7WebCore14LoggedInStatus6createERKNS_17RegistrableDomainERKN3WTF6StringENS0_19CredentialTokenTypeENS0_18AuthenticationTypeENS4_7SecondsE(&domain, &username, false, authType, WTF::Seconds{timeToLive});
}

static void CreatesWithDefaultAndClampedLifetimes() {
    RegistrableDomain domain{Latin1("example.com")};
    const auto username = Latin1("player.one");
    const double before = Now();
    auto unmanaged = Create(domain, username, Unmanaged);
    Require(unmanaged.hasValue && unmanaged.value);
    const auto& status = *unmanaged.value;
    Require(status.domain.string.impl == domain.string.impl && status.username.impl == username.impl);
    Require(status.tokenType && status.authType == Unmanaged && status.timeToLive.m_value == Week);
    Require(status.loggedInTime.m_value >= before && status.loggedInTime.m_value <= Now());
    Require(Create(domain, username, WebAuthn).value->timeToLive.m_value == NinetyDays);
    auto shorter = Create(domain, username, PasswordManager, 60.0);
    Require(shorter.hasValue && !shorter.value->tokenType && shorter.value->timeToLive.m_value == 60.0);
    Require(Create(domain, username, Unmanaged, NinetyDays).value->timeToLive.m_value == Week);
    Require(std::isnan(Create(domain, username, Unmanaged, std::nan("")).value->timeToLive.m_value));
}

static void RefusesInvalidInput() {
    RegistrableDomain domain{Latin1("example.com")};
    RegistrableDomain empty;
    const auto name = Latin1("user");
    auto noDomain = Create(empty, name, Unmanaged);
    Require(!noDomain.hasValue && noDomain.exception.m_code == 14);
    Require(Is(noDomain.exception.m_message, "IsLoggedIn status can only be set for origins with a registrable domain."));
    auto noName = Create(domain, String(), Unmanaged);
    Require(!noName.hasValue && noName.exception.m_code == 9 && Is(noName.exception.m_message, "IsLoggedIn requires a non-empty username."));
    const std::string sixtyFour(64, 'a');
    Require(Create(domain, Latin1(sixtyFour.c_str()), Unmanaged).hasValue);
    auto tooLong = Create(domain, Latin1((sixtyFour + "a").c_str()), Unmanaged);
    Require(!tooLong.hasValue && tooLong.exception.m_code == 9 && Is(tooLong.exception.m_message, "IsLoggedIn usernames cannot be longer than 64"));
    for (const char* spaced : {"a b", "a\tb", "a\nb", "a\vb", "a\fb", "a\rb"}) {
        auto result = Create(domain, Latin1(spaced), Unmanaged);
        Require(!result.hasValue && result.exception.m_code == 3 && Is(result.exception.m_message, "IsLoggedIn usernames cannot contain whitespace or newlines."));
    }
    const char16_t ideographic[] = {u'a', 0x3000, u'b'};
    Require(!Create(domain, Utf16(ideographic, 3), Unmanaged).hasValue);
    const char16_t noBreak[] = {u'a', 0x00A0, u'b'};
    Require(Create(domain, Utf16(noBreak, 3), Unmanaged).hasValue);
    const char16_t accented[] = {u'r', 0x00E9, 0x4E2D};
    Require(Create(domain, Utf16(accented, 3), Unmanaged).hasValue);
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore14LoggedInStatus6createERKNS_17RegistrableDomainERKN3WTF6StringENS0_19CredentialTokenTypeENS0_18AuthenticationTypeE(nullptr, &name, false, 0); }));
}

static void UpdatesAndReportsExpiry() {
    RegistrableDomain domain{Latin1("example.com")};
    auto result = Create(domain, Latin1("user"), PasswordManager);
    auto* status = result.value;
    _ZN7WebCore14LoggedInStatus13setTimeToLiveEN3WTF7SecondsE(status, WTF::Seconds{3600.0});
    Require(status->timeToLive.m_value == 3600.0);
    _ZN7WebCore14LoggedInStatus13setTimeToLiveEN3WTF7SecondsE(status, WTF::Seconds{NinetyDays * 2});
    Require(status->timeToLive.m_value == NinetyDays);
    Require(!_ZNK7WebCore14LoggedInStatus10hasExpiredEv(status));
    status->loggedInTime.m_value = Now() - NinetyDays - 1;
    Require(_ZNK7WebCore14LoggedInStatus10hasExpiredEv(status));
    const double before = Now();
    const double expiry = _ZNK7WebCore14LoggedInStatus6expiryEv(status).m_value;
    Require(expiry >= before + NinetyDays && expiry <= Now() + NinetyDays);
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore14LoggedInStatus10hasExpiredEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore14LoggedInStatus6expiryEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore14LoggedInStatus13setTimeToLiveEN3WTF7SecondsE(nullptr, WTF::Seconds{1.0}); }));
}

int main() {
    CreatesWithDefaultAndClampedLifetimes();
    RefusesInvalidInput();
    UpdatesAndReportsExpiry();
    std::puts("LoggedInStatus passed");
    return 0;
}
