#pragma once

#include "SceTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct BackForwardItemIdentifier {
    std::uint64_t processIdentifier;
    std::uint64_t itemIdentifier;
};

static_assert(sizeof(BackForwardItemIdentifier) == 16);
static_assert(offsetof(BackForwardItemIdentifier, itemIdentifier) == 8);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

bool APS5_VABI _ZNK7WebCore25BackForwardItemIdentifier7isValidEv(const NKWebKit::WebCoreAbi::BackForwardItemIdentifier* self);

}
