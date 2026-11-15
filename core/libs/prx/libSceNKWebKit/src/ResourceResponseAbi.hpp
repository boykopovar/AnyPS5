#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/CacheControlAbi.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/ParsedContentRangeAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct StringSlot {
    WtfAbi::StringImpl* impl;
};

using ResponseHeaderStorage = HttpAbi::HTTPHeaderMap;

struct ResponseOptionalTime {
    bool hasValue{};
    double seconds{};

    ResponseOptionalTime() = default;
    ResponseOptionalTime(bool engaged, double value) : hasValue(engaged), seconds(value) {}
    ResponseOptionalTime(const ResponseOptionalTime& other) : hasValue(other.hasValue), seconds(other.seconds) {}
    ResponseOptionalTime(ResponseOptionalTime&& other) noexcept : ResponseOptionalTime(other) {}
};

struct ByteVector {
    std::uint8_t* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct CertificateChain {
    ByteVector* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct CertificateInfo {
    int verificationError;
    CertificateChain certificateChain;
    ByteVector privateKey;
    StringSlot privateKeyPassword;
};

struct OptionalCertificateInfo {
    CertificateInfo value;
    bool hasValue;
};

enum class ResponseType : std::uint8_t { Basic, Cors, Default, Error, Opaque, Opaqueredirect };
enum class ResponseTainting : std::uint8_t { Basic, Cors, Opaque, Opaqueredirect };
enum class ResponseSource : std::uint8_t {
    Unknown, Network, DiskCache, DiskCacheAfterValidation, MemoryCache,
    MemoryCacheAfterValidation, ServiceWorker, ApplicationCache, DOMCache, InspectorOverride
};
enum class ResponseSanitizationType { Redirection, RemoveCookies, CrossOriginSafe };
enum ResponseInitLevel { Uninitialized, CommonFieldsOnly, AllFields };

inline constexpr std::uint8_t responseNullMask = 0x01;
inline constexpr unsigned responseTaintingShift = 0;
inline constexpr std::uint8_t responseTaintingMask = 0x03;
inline constexpr unsigned responseSourceShift = 2;
inline constexpr std::uint8_t responseSourceMask = 0x0f;
inline constexpr unsigned responseTypeShift = 0;
inline constexpr std::uint8_t responseTypeMask = 0x07;

struct ResourceResponseBase {
    static constexpr double emptyTime = std::numeric_limits<double>::quiet_NaN();

    WtfAbi::URL url;
    StringSlot mimeType;
    long long expectedContentLength;
    StringSlot textEncodingName;
    StringSlot httpStatusText;
    StringSlot httpVersion;
    ResponseHeaderStorage httpHeaderFields;
    void* networkLoadMetrics;
    OptionalCertificateInfo certificateInfo;
    short httpStatusCode;
    std::uint8_t stateBits{responseNullMask};
    double age = emptyTime;
    double date = emptyTime;
    double expires = emptyTime;
    double lastModified = emptyTime;
    ParsedContentRange contentRange{0, 0, ParsedContentRange::invalidLength};
    HttpAbi::CacheControlDirectives cacheControlDirectives{};
    std::uint8_t parseBits{};
    std::uint8_t taintingSourceBits{};
    std::uint8_t typeBits{static_cast<std::uint8_t>(ResponseType::Default)};
};

static_assert(sizeof(StringSlot) == 8);
static_assert(sizeof(ResponseHeaderStorage) == 32);
static_assert(sizeof(ResponseOptionalTime) == 16);
static_assert(offsetof(ResponseOptionalTime, hasValue) == 0);
static_assert(offsetof(ResponseOptionalTime, seconds) == 8);
static_assert(!std::is_trivially_copy_constructible_v<ResponseOptionalTime>);
static_assert(!std::is_trivially_move_constructible_v<ResponseOptionalTime>);
static_assert(std::is_trivially_destructible_v<ResponseOptionalTime>);
static_assert(offsetof(ResourceResponseBase, url) == 0);
static_assert(offsetof(ResourceResponseBase, mimeType) == 40);
static_assert(offsetof(ResourceResponseBase, expectedContentLength) == 48);
static_assert(offsetof(ResourceResponseBase, textEncodingName) == 56);
static_assert(offsetof(ResourceResponseBase, httpStatusText) == 64);
static_assert(offsetof(ResourceResponseBase, httpVersion) == 72);
static_assert(offsetof(ResourceResponseBase, httpHeaderFields) == 80);
static_assert(sizeof(ByteVector) == 16);
static_assert(sizeof(CertificateChain) == 16);
static_assert(sizeof(CertificateInfo) == 48);
static_assert(offsetof(CertificateInfo, verificationError) == 0);
static_assert(offsetof(CertificateInfo, certificateChain) == 8);
static_assert(offsetof(CertificateInfo, privateKey) == 24);
static_assert(offsetof(CertificateInfo, privateKeyPassword) == 40);
static_assert(sizeof(OptionalCertificateInfo) == 56);
static_assert(offsetof(OptionalCertificateInfo, hasValue) == 48);
static_assert(offsetof(ResourceResponseBase, networkLoadMetrics) == 112);
static_assert(offsetof(ResourceResponseBase, certificateInfo) == 120);
static_assert(offsetof(ResourceResponseBase, httpStatusCode) == 176);
static_assert(offsetof(ResourceResponseBase, stateBits) == 178);
static_assert(offsetof(ResourceResponseBase, age) == 184);
static_assert(offsetof(ResourceResponseBase, date) == 192);
static_assert(offsetof(ResourceResponseBase, expires) == 200);
static_assert(offsetof(ResourceResponseBase, lastModified) == 208);
static_assert(offsetof(ResourceResponseBase, contentRange) == 216);
static_assert(offsetof(ResourceResponseBase, cacheControlDirectives) == 240);
static_assert(offsetof(ResourceResponseBase, parseBits) == 272);
static_assert(offsetof(ResourceResponseBase, taintingSourceBits) == 273);
static_assert(offsetof(ResourceResponseBase, typeBits) == 274);
static_assert(sizeof(ResourceResponseBase) == 280);
static_assert(alignof(ResourceResponseBase) == 8);

}
