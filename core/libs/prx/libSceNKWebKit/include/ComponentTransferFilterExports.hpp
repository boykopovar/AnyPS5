#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit {

enum class LegacyFilterOperationType : std::int32_t {
    Reference,
    Grayscale,
    Sepia,
    Saturate,
    HueRotate,
    Invert,
    AppleInvertLightness,
    Opacity,
    Brightness,
    Contrast,
    Blur,
    DropShadow,
    Passthrough,
    Default,
    None,
};

struct ComponentTransferFilterOperationAbi {
    const void* vptr;
    std::uint32_t refCount;
    LegacyFilterOperationType type;
    double amount;
};

static_assert(sizeof(ComponentTransferFilterOperationAbi) == 24);
static_assert(offsetof(ComponentTransferFilterOperationAbi, type) == 12);
static_assert(offsetof(ComponentTransferFilterOperationAbi, amount) == 16);

}  // namespace NKWebKit

extern "C" {

bool APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(const NKWebKit::ComponentTransferFilterOperationAbi* self, WebCore::SRGBA<float>* color);

}
