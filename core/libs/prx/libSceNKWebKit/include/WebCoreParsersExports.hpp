#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WebCore {

enum class XFrameOptionsDisposition : std::uint8_t { None, Deny, SameOrigin, AllowAll, Invalid, Conflict };

enum class HTMLIntegerParsingError : int { NegativeOverflow, PositiveOverflow, Other };

struct ExpectedInt {
    union {
        int value;
        HTMLIntegerParsingError error;
    };
    bool hasValue;
};

enum class CrossOriginResourcePolicy : std::uint8_t { None, CrossOrigin, SameOrigin, SameSite, Invalid };

enum class Mode : std::int32_t { Rfc2045, MimeSniff };

static_assert(sizeof(ExpectedInt) == 8);
static_assert(offsetof(ExpectedInt, hasValue) == 4);
static_assert(std::is_trivially_copyable_v<ExpectedInt>);
static_assert(sizeof(Mode) == 4);

}  // namespace WebCore

namespace NKWebKit::WtfAbi {

struct ByteVector {
    unsigned char* buffer{nullptr};
    unsigned capacity{0};
    unsigned size{0};

    ByteVector() = default;
    ByteVector(const ByteVector&) = delete;
    ByteVector& operator=(const ByteVector&) = delete;
    ByteVector(ByteVector&& other) noexcept;
    ~ByteVector();
};

static_assert(sizeof(ByteVector) == 16);
static_assert(offsetof(ByteVector, capacity) == 8);
static_assert(offsetof(ByteVector, size) == 12);

}  // namespace NKWebKit::WtfAbi

extern "C" {

WebCore::XFrameOptionsDisposition APS5_VABI _ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(const NKWebKit::WtfAbi::String& header);
WebCore::ExpectedInt APS5_VABI _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(NKWebKit::WtfAbi::StringView input);
WebCore::CrossOriginResourcePolicy APS5_VABI _ZN7WebCore36parseCrossOriginResourcePolicyHeaderEN3WTF10StringViewE(NKWebKit::WtfAbi::StringView header);
bool APS5_VABI _ZN7WebCore18isValidContentTypeERKN3WTF6StringENS_4ModeE(const NKWebKit::WtfAbi::String& input, WebCore::Mode mode);
bool APS5_VABI _ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(const NKWebKit::WtfAbi::String* range, long long* rangeOffset, long long* rangeEnd, long long* rangeSuffixLength);
NKWebKit::WtfAbi::ByteVector APS5_VABI _ZN3WTF24normalizeLineEndingsToLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(NKWebKit::WtfAbi::ByteVector&& vector);
NKWebKit::WtfAbi::ByteVector APS5_VABI _ZN3WTF26normalizeLineEndingsToCRLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(NKWebKit::WtfAbi::ByteVector&& source);
NKWebKit::WtfAbi::ByteVector APS5_VABI _ZN3WTF28normalizeLineEndingsToNativeEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(NKWebKit::WtfAbi::ByteVector&& from);

}
