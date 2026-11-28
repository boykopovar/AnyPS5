#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIcu/Properties.hpp"
#include "prx/libSceNKWebKit/include/LoggedInStatusExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

namespace NKWebKit::WebCoreAbi {

LoggedInStatusOrException::~LoggedInStatusOrException() {
    if (!hasValue) {
        exception.~Exception();
        return;
    }
    value->~LoggedInStatus();
    _ZN3WTF8fastFreeEPv(value);
}

}  // namespace NKWebKit::WebCoreAbi

namespace {

using NKWebKit::WebCoreAbi::LoggedInStatus;
using NKWebKit::WebCoreAbi::LoggedInStatusOrException;
using NKWebKit::WebCoreAbi::RegistrableDomain;
using NKWebKit::WtfAbi::String;

constexpr int InvalidCharacterError = 3;
constexpr int SyntaxError = 9;
constexpr int SecurityError = 14;
constexpr unsigned UsernameMaxLength = 64;
constexpr double TimeToLiveShort = 24.0 * 60 * 60 * 7;
constexpr double TimeToLiveLong = 24.0 * 60 * 60 * 90;
constexpr std::uint8_t Unmanaged = 2;
constexpr std::int32_t WhiteSpaceNeutral = 9;

String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

bool IsEmpty(const String& string) {
    return !string.impl || !string.impl->length;
}

char16_t CharacterAt(const String& string, unsigned index) {
    return string.impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(string.impl->data)[index] : static_cast<const char16_t*>(string.impl->data)[index];
}

bool IsSpaceOrNewline(char16_t character) {
    if (character <= 0xFF) return character == ' ' || character == '\n' || character == '\t' || character == '\r' || character == '\f' || character == '\v';
    return u_charDirection_nid_postfix(character) == WhiteSpaceNeutral;
}

double Cap(std::uint8_t authType) {
    return authType == Unmanaged ? TimeToLiveShort : TimeToLiveLong;
}

void SetTimeToLive(LoggedInStatus& status, WTF::Seconds timeToLive) {
    const double cap = Cap(status.authType);
    status.timeToLive.m_value = cap < timeToLive.m_value ? cap : timeToLive.m_value;
}

LoggedInStatusOrException Create(const RegistrableDomain* domain, const String* username, bool tokenType, std::uint8_t authType, WTF::Seconds timeToLive) {
    if (!domain || !username) throw std::invalid_argument("WebCore::LoggedInStatus::create: null domain or username");
    if (IsEmpty(domain->string)) return {SecurityError, Latin1("IsLoggedIn status can only be set for origins with a registrable domain.")};
    if (IsEmpty(*username)) return {SyntaxError, Latin1("IsLoggedIn requires a non-empty username.")};
    if (username->impl->length > UsernameMaxLength) return {SyntaxError, Latin1(("IsLoggedIn usernames cannot be longer than " + std::to_string(UsernameMaxLength)).c_str())};
    for (unsigned index = 0; index < username->impl->length; ++index) {
        if (IsSpaceOrNewline(CharacterAt(*username, index))) return {InvalidCharacterError, Latin1("IsLoggedIn usernames cannot contain whitespace or newlines.")};
    }
    auto* status = new (_ZN3WTF10fastMallocEm(sizeof(LoggedInStatus))) LoggedInStatus{{domain->string}, *username, tokenType, authType, _ZN3WTF8WallTime3nowEv(), {}};
    SetTimeToLive(*status, timeToLive);
    return LoggedInStatusOrException(status);
}

const LoggedInStatus& Checked(const LoggedInStatus* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::LoggedInStatus::") + operation + ": null this");
    return *self;
}

}  // namespace

extern "C" {

LoggedInStatusOrException APS5_VABI _ZN7WebCore14LoggedInStatus6createERKNS_17RegistrableDomainERKN3WTF6StringENS0_19CredentialTokenTypeENS0_18AuthenticationTypeE(const RegistrableDomain* domain, const String* username, bool tokenType, std::uint8_t authType) {
    return Create(domain, username, tokenType, authType, WTF::Seconds{Cap(authType)});
}

LoggedInStatusOrException APS5_VABI _ZN7WebCore14LoggedInStatus6createERKNS_17RegistrableDomainERKN3WTF6StringENS0_19CredentialTokenTypeENS0_18AuthenticationTypeENS4_7SecondsE(const RegistrableDomain* domain, const String* username, bool tokenType, std::uint8_t authType, WTF::Seconds timeToLive) {
    return Create(domain, username, tokenType, authType, timeToLive);
}

void APS5_VABI _ZN7WebCore14LoggedInStatus13setTimeToLiveEN3WTF7SecondsE(LoggedInStatus* self, WTF::Seconds timeToLive) {
    Checked(self, "setTimeToLive");
    SetTimeToLive(*self, timeToLive);
}

bool APS5_VABI _ZNK7WebCore14LoggedInStatus10hasExpiredEv(const LoggedInStatus* self) {
    const auto& status = Checked(self, "hasExpired");
    return _ZN3WTF8WallTime3nowEv().m_value > status.loggedInTime.m_value + status.timeToLive.m_value;
}

WTF::WallTime APS5_VABI _ZNK7WebCore14LoggedInStatus6expiryEv(const LoggedInStatus* self) {
    const auto& status = Checked(self, "expiry");
    return WTF::WallTime{_ZN3WTF8WallTime3nowEv().m_value + status.timeToLive.m_value};
}

}
