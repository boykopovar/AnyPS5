#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

enum class ResourceErrorType : std::uint8_t {
    Null,
    General,
    AccessControl,
    Cancellation,
    Timeout
};

struct ResourceError {
    WtfAbi::String domain;
    WtfAbi::URL failingURL{};
    WtfAbi::String localizedDescription;
    int errorCode{0};
    ResourceErrorType type{ResourceErrorType::Null};
    std::uint8_t isSanitized{0};
    int tlsVerificationError{0};
    WtfAbi::String tlsVerificationErrorDescription;
};

static_assert(sizeof(ResourceError) == 80);
static_assert(alignof(ResourceError) == 8);
static_assert(offsetof(ResourceError, domain) == 0);
static_assert(offsetof(ResourceError, failingURL) == 8);
static_assert(offsetof(ResourceError, localizedDescription) == 48);
static_assert(offsetof(ResourceError, errorCode) == 56);
static_assert(offsetof(ResourceError, type) == 60);
static_assert(offsetof(ResourceError, isSanitized) == 61);
static_assert(offsetof(ResourceError, tlsVerificationError) == 64);
static_assert(offsetof(ResourceError, tlsVerificationErrorDescription) == 72);
static_assert(static_cast<std::uint8_t>(ResourceErrorType::Null) == 0);
static_assert(static_cast<std::uint8_t>(ResourceErrorType::General) == 1);
static_assert(static_cast<std::uint8_t>(ResourceErrorType::AccessControl) == 2);
static_assert(static_cast<std::uint8_t>(ResourceErrorType::Cancellation) == 3);
static_assert(static_cast<std::uint8_t>(ResourceErrorType::Timeout) == 4);
static_assert(std::is_trivially_destructible_v<ResourceErrorType>);
static_assert(!std::is_trivially_copyable_v<ResourceError>);

}  // namespace NKWebKit::WebCoreAbi
