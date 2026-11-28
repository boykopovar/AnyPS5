#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::UrlAbi {

struct StringView {
    const void* characters;
    std::uint32_t length;
    bool is8Bit;
};

static_assert(sizeof(StringView) == 16);
static_assert(offsetof(StringView, characters) == 0);
static_assert(offsetof(StringView, length) == 8);
static_assert(offsetof(StringView, is8Bit) == 12);

struct OptionalString {
    bool hasValue{false};
    WtfAbi::String value{nullptr};
};

struct FormField {
    WtfAbi::String key;
    WtfAbi::String value;
};

using FormFields = WtfAbi::Vector<FormField>;

static_assert(sizeof(OptionalString) == 16);
static_assert(offsetof(OptionalString, hasValue) == 0);
static_assert(offsetof(OptionalString, value) == 8);
static_assert(!std::is_trivially_destructible_v<OptionalString>);
static_assert(sizeof(FormField) == 16);
static_assert(offsetof(FormField, value) == 8);
static_assert(sizeof(FormFields) == 16);
static_assert(offsetof(FormFields, capacity) == 8);
static_assert(offsetof(FormFields, size) == 12);
static_assert(!std::is_trivially_destructible_v<FormFields>);

} // namespace NKWebKit::UrlAbi

extern "C" {

bool APS5_VABI _ZN3WTF14isVersion4UUIDENS_10StringViewE(NKWebKit::UrlAbi::StringView value);
bool APS5_VABI _ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(std::uint16_t port, NKWebKit::UrlAbi::StringView protocol);
void APS5_VABI _ZN3WTF40clearDefaultPortForProtocolMapForTestingEv();
void APS5_VABI _ZN3WTF40registerDefaultPortForProtocolForTestingEtRKNS_6StringE(std::uint16_t port, const NKWebKit::WtfAbi::String& protocol);
NKWebKit::UrlAbi::OptionalString APS5_VABI _ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(const NKWebKit::WtfAbi::String& scheme);
NKWebKit::UrlAbi::FormFields APS5_VABI _ZN3WTF9URLParser19parseURLEncodedFormENS_10StringViewE(NKWebKit::UrlAbi::StringView input);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF9URLParser9serializeERKNS_6VectorINS_12KeyValuePairINS_6StringES3_EELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(const NKWebKit::UrlAbi::FormFields& fields);

}
