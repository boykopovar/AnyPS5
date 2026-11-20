#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

enum class ResourceErrorType : std::uint8_t { Null, General, AccessControl, Cancellation, Timeout };

struct ResourceError {
    WtfAbi::StringImpl* domain;
    WtfAbi::URL failingURL;
    WtfAbi::StringImpl* localizedDescription;
    int errorCode;
    ResourceErrorType type;
    bool isSanitized;
    int tlsVerificationError;
    WtfAbi::StringImpl* tlsVerificationErrorDescription;
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
static_assert(std::is_trivially_copyable_v<ResourceError>);

inline ResourceError EmptyResourceError(ResourceErrorType type) {
    ResourceError error {};
    error.type = type;
    return error;
}

}
