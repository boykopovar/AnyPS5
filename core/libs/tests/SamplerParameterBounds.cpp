#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

int failures = 0;

struct CapturedSampler {
    bool created = false;
    float maxAnisotropy = 0.0f;
    float mipLodBias = 0.0f;
    bool anisotropyEnable = false;
    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
};
CapturedSampler capturedSampler{};

template<typename THandle>
THandle fakeHandle(std::uint64_t value) {
    if constexpr (std::is_pointer_v<THandle>) return reinterpret_cast<THandle>(static_cast<std::uintptr_t>(value));
    else return static_cast<THandle>(value);
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateSampler(VkDevice, const VkSamplerCreateInfo* info, const VkAllocationCallbacks*, VkSampler* sampler) {
    capturedSampler.created = true;
    capturedSampler.maxAnisotropy = info->maxAnisotropy;
    capturedSampler.mipLodBias = info->mipLodBias;
    capturedSampler.anisotropyEnable = info->anisotropyEnable == VK_TRUE;
    capturedSampler.addressModeU = info->addressModeU;
    capturedSampler.addressModeV = info->addressModeV;
    capturedSampler.addressModeW = info->addressModeW;
    capturedSampler.mipmapMode = info->mipmapMode;
    *sampler = fakeHandle<VkSampler>(0x42);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroySampler(VkDevice, VkSampler, const VkAllocationCallbacks*) {}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    if (std::strcmp(name, "vkCreateSampler") == 0) return reinterpret_cast<PFN_vkVoidFunction>(mockCreateSampler);
    if (std::strcmp(name, "vkDestroySampler") == 0) return reinterpret_cast<PFN_vkVoidFunction>(mockDestroySampler);
    return nullptr;
}

void ExpectFailure(const char* name, auto&& function, std::string_view expected) {
    try {
        function();
        std::fprintf(stderr, "%s: expected exception containing '%s', but none was thrown\n", name, expected.data());
        ++failures;
    } catch (const std::exception& error) {
        if (std::string_view(error.what()).find(expected) == std::string_view::npos) {
            std::fprintf(stderr, "%s: exception '%s' does not contain '%s'\n", name, error.what(), expected.data());
            ++failures;
        }
    }
}

void ExpectSuccess(const char* name, auto&& function) {
    try {
        function();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: unexpected exception: %s\n", name, error.what());
        ++failures;
    }
}

AgcDriver::Graphics::Context BaseContext() {
    AgcDriver::Graphics::Context context{};
    context.deviceProc = mockProc;
    context.limits.maxSamplerAnisotropy = 16.0f;
    context.limits.maxSamplerLodBias = 15.0f;
    context.samplerAnisotropy = true;
    context.samplerMirrorClampToEdge = true;
    return context;
}

AgcDriver::Graphics::GuestSamplerResource BaseDescriptor() {
    AgcDriver::Graphics::GuestSamplerResource resource{};
    resource.magFilter = VK_FILTER_LINEAR;
    resource.minFilter = VK_FILTER_LINEAR;
    resource.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    resource.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    resource.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    resource.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    resource.minLod = 0.0f;
    resource.maxLod = 16.0f;
    resource.lodBias = 0.0f;
    resource.anisotropyEnable = false;
    resource.maxAnisotropy = 1.0f;
    return resource;
}

void TestValidAddressModes() {
    auto context = BaseContext();
    for (const auto mode : {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE}) {
        auto desc = BaseDescriptor();
        desc.addressModeU = mode;
        desc.addressModeV = mode;
        desc.addressModeW = mode;
        ExpectSuccess("ValidAddressModes", [&] {
            AgcDriver::Graphics::Sampler sampler(context, desc);
        });
    }
}

void TestInvalidAddressModes() {
    auto context = BaseContext();
    auto badU = BaseDescriptor();
    badU.addressModeU = static_cast<VkSamplerAddressMode>(99);
    ExpectFailure("InvalidAddressModeU", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, badU);
    }, "invalid address mode");

    auto badV = BaseDescriptor();
    badV.addressModeV = static_cast<VkSamplerAddressMode>(100);
    ExpectFailure("InvalidAddressModeV", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, badV);
    }, "invalid address mode");

    auto badW = BaseDescriptor();
    badW.addressModeW = static_cast<VkSamplerAddressMode>(101);
    ExpectFailure("InvalidAddressModeW", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, badW);
    }, "invalid address mode");
}

void TestMirrorClampToEdge() {
    auto context = BaseContext();
    context.samplerMirrorClampToEdge = false;

    auto badU = BaseDescriptor();
    badU.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    ExpectFailure("MirrorClampUnsupportedU", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, badU);
    }, "mirror clamp to edge");

    auto badV = BaseDescriptor();
    badV.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    ExpectFailure("MirrorClampUnsupportedV", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, badV);
    }, "mirror clamp to edge");

    auto badW = BaseDescriptor();
    badW.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    ExpectFailure("MirrorClampUnsupportedW", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, badW);
    }, "mirror clamp to edge");

    context.samplerMirrorClampToEdge = true;
    ExpectSuccess("MirrorClampSupported", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, badU);
        AgcDriver::Graphics::Sampler::Validate(context, badV);
        AgcDriver::Graphics::Sampler::Validate(context, badW);
    });
}

void TestInvalidMipmapMode() {
    auto context = BaseContext();
    auto desc = BaseDescriptor();
    desc.mipmapMode = static_cast<VkSamplerMipmapMode>(99);
    ExpectFailure("InvalidMipmapMode", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, desc);
    }, "invalid mipmap mode");
}

void TestInvalidFilterModes() {
    auto context = BaseContext();
    auto badMag = BaseDescriptor();
    badMag.magFilter = static_cast<VkFilter>(99);
    ExpectFailure("InvalidMagFilter", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, badMag);
    }, "invalid mag filter");

    auto badMin = BaseDescriptor();
    badMin.minFilter = static_cast<VkFilter>(99);
    ExpectFailure("InvalidMinFilter", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, badMin);
    }, "invalid min filter");
}

void TestLodBiasBounds() {
    auto context = BaseContext();
    auto high = BaseDescriptor();
    high.lodBias = 16.0f;
    ExpectFailure("LodBiasHigh", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, high);
    }, "requests a LOD bias beyond the device limit");

    auto low = BaseDescriptor();
    low.lodBias = -16.0f;
    ExpectFailure("LodBiasLow", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, low);
    }, "requests a LOD bias beyond the device limit");

    auto nanBias = BaseDescriptor();
    nanBias.lodBias = std::numeric_limits<float>::quiet_NaN();
    ExpectFailure("LodBiasNaN", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, nanBias);
    }, "requests a LOD bias beyond the device limit");

    auto atLimit = BaseDescriptor();
    atLimit.lodBias = 15.0f;
    ExpectSuccess("LodBiasAtLimit", [&] {
        AgcDriver::Graphics::Sampler sampler(context, atLimit);
    });
}

void TestLodRangeBounds() {
    auto context = BaseContext();
    auto inverted = BaseDescriptor();
    inverted.minLod = 10.0f;
    inverted.maxLod = 2.0f;
    ExpectFailure("LodRangeInverted", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, inverted);
    }, "minimum LOD past its maximum LOD");

    auto negativeMin = BaseDescriptor();
    negativeMin.minLod = -1.0f;
    ExpectFailure("LodRangeNegativeMin", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, negativeMin);
    }, "minimum LOD past its maximum LOD");

    auto nanLod = BaseDescriptor();
    nanLod.minLod = std::numeric_limits<float>::quiet_NaN();
    ExpectFailure("LodRangeNaN", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, nanLod);
    }, "minimum LOD past its maximum LOD");
}

void TestAnisotropyClampingAndBounds() {
    auto context = BaseContext();
    context.limits.maxSamplerAnisotropy = 4.0f;

    auto desc = BaseDescriptor();
    desc.anisotropyEnable = true;
    desc.maxAnisotropy = 4.0f;
    capturedSampler = {};
    ExpectSuccess("AnisotropyValid", [&] {
        AgcDriver::Graphics::Sampler sampler(context, desc);
    });
    if (!capturedSampler.created || !capturedSampler.anisotropyEnable || capturedSampler.maxAnisotropy != 4.0f) {
        std::fprintf(stderr, "AnisotropyValid: unexpected capture values\n");
        ++failures;
    }

    auto disabledAboveLimit = BaseDescriptor();
    disabledAboveLimit.anisotropyEnable = false;
    disabledAboveLimit.maxAnisotropy = 32.0f;
    ExpectSuccess("AnisotropyDisabledAboveLimit", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, disabledAboveLimit);
    });
    capturedSampler = {};
    ExpectSuccess("AnisotropyDisabledClampedToOne", [&] {
        AgcDriver::Graphics::Sampler sampler(context, disabledAboveLimit);
    });
    if (!capturedSampler.created || capturedSampler.anisotropyEnable || capturedSampler.maxAnisotropy != 1.0f) {
        std::fprintf(stderr, "AnisotropyDisabledClampedToOne: unexpected capture values\n");
        ++failures;
    }

    auto belowOne = BaseDescriptor();
    belowOne.anisotropyEnable = true;
    belowOne.maxAnisotropy = 0.5f;
    ExpectFailure("AnisotropyBelowOne", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, belowOne);
    }, "requests an anisotropy ratio below 1.0");

    auto aboveLimit = BaseDescriptor();
    aboveLimit.anisotropyEnable = true;
    aboveLimit.maxAnisotropy = 8.0f;
    ExpectFailure("AnisotropyAboveLimit", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, aboveLimit);
    }, "requests an anisotropy ratio beyond the device limit");

    context.samplerAnisotropy = false;
    ExpectFailure("AnisotropyUnsupported", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, desc);
    }, "requests anisotropic filtering which the device does not support");
}

void TestUnnormalizedCoordinatesRestrictions() {
    auto context = BaseContext();
    auto validUnnorm = BaseDescriptor();
    validUnnorm.unnormalizedCoordinates = true;
    validUnnorm.anisotropyEnable = false;
    validUnnorm.minFilter = VK_FILTER_NEAREST;
    validUnnorm.magFilter = VK_FILTER_NEAREST;
    validUnnorm.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    validUnnorm.minLod = 0.0f;
    validUnnorm.maxLod = 0.0f;
    validUnnorm.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    validUnnorm.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    ExpectSuccess("ValidUnnormalized", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, validUnnorm);
    });

    auto unnormAniso = validUnnorm;
    unnormAniso.anisotropyEnable = true;
    ExpectFailure("UnnormalizedAniso", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, unnormAniso);
    }, "unnormalized coordinates enables anisotropic filtering");

    auto unnormMismatched = validUnnorm;
    unnormMismatched.magFilter = VK_FILTER_LINEAR;
    ExpectFailure("UnnormalizedMismatchedFilters", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, unnormMismatched);
    }, "matching min and mag filters");

    auto unnormLinear = validUnnorm;
    unnormLinear.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    ExpectFailure("UnnormalizedLinearMipmap", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, unnormLinear);
    }, "unnormalized coordinates uses linear mipmap mode");

    auto unnormNonZeroMinLod = validUnnorm;
    unnormNonZeroMinLod.minLod = 1.0f;
    unnormNonZeroMinLod.maxLod = 1.0f;
    ExpectFailure("UnnormalizedNonZeroMinLod", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, unnormNonZeroMinLod);
    }, "zero min and max LOD");

    auto unnormNonZeroMaxLod = validUnnorm;
    unnormNonZeroMaxLod.maxLod = 1.0f;
    ExpectFailure("UnnormalizedNonZeroMaxLod", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, unnormNonZeroMaxLod);
    }, "zero min and max LOD");

    auto unnormRepeatU = validUnnorm;
    unnormRepeatU.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ExpectFailure("UnnormalizedRepeatU", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, unnormRepeatU);
    }, "unsupported address mode on U");

    auto unnormRepeatV = validUnnorm;
    unnormRepeatV.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ExpectFailure("UnnormalizedRepeatV", [&] {
        AgcDriver::Graphics::Sampler::Validate(context, unnormRepeatV);
    }, "unsupported address mode on V");
}

}

int main() {
    TestValidAddressModes();
    TestInvalidAddressModes();
    TestMirrorClampToEdge();
    TestInvalidMipmapMode();
    TestInvalidFilterModes();
    TestLodBiasBounds();
    TestLodRangeBounds();
    TestAnisotropyClampingAndBounds();
    TestUnnormalizedCoordinatesRestrictions();
    return failures == 0 ? 0 : 1;
}
