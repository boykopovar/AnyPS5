#include "BdaAbi.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::uint32_t TwoChannelSwizzle = 0x22cu;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TexelBytes = 8192;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) constexpr std::array<std::uint32_t, 12> StoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x0001141e,
    0x7e3e0281, 0xf0201508, 0x0001141e, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 12> StoreD16Code{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x8001141e,
    0x7e3e0281, 0xf0201508, 0x8001141e, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 19> MixedCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x0001141e,
    0x7e3e0281, 0xf0201508, 0x0001141e, 0x7e3e0282, 0xf0001f08, 0x0001181e, 0xbf8c3f70, 0x7e3e0283,
    0xf0201f08, 0x0001181e, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 19> MixedD16Code{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x8001141e,
    0x7e3e0281, 0xf0201508, 0x8001141e, 0x7e3e0282, 0xf0001f08, 0x8001181e, 0xbf8c3f70, 0x7e3e0283,
    0xf0201f08, 0x8001181e, 0xbf810000,
};

struct Format {
    const char* name;
    std::uint32_t value;
    std::uint32_t components;
};

constexpr std::array<Format, 3> Formats{{
    {"16_FLOAT", 13u, 1u},
    {"16_16_FLOAT", 29u, 2u},
    {"16_16_16_16_FLOAT", 71u, 4u},
}};

struct Write {
    std::uint32_t row;
    std::uint32_t dmask;
};

constexpr std::array<Write, 2> Writes{{{0u, 0xfu}, {1u, 0x5u}}};
constexpr std::array<std::uint32_t, 24> EdgeBits{
    0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x3f801000u, 0x3f803000u,
    0x3f800fffu, 0x3f801001u, 0x477fe000u, 0x477ff000u, 0x47800000u, 0xc7800000u,
    0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u, 0x38800000u, 0x33800000u,
    0x33000000u, 0x000116c2u, 0x3eaaaaabu, 0x40490fdbu, 0x00000001u, 0x80000001u,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image store float rounding: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::uint16_t HalfTowardZero(std::uint32_t bits) {
    const auto sign = static_cast<std::uint16_t>((bits >> 16u) & 0x8000u);
    const auto exponent = (bits >> 23u) & 0xffu;
    const auto mantissa = bits & 0x7fffffu;
    if (exponent == 0xffu) {
        auto payload = static_cast<std::uint16_t>(mantissa >> 13u);
        if (mantissa != 0u && payload == 0u) payload = 1u;
        return static_cast<std::uint16_t>(sign | 0x7c00u | payload);
    }
    if (exponent == 0u) return sign;
    const int unbiased = static_cast<int>(exponent) - 127;
    if (unbiased > 15) return static_cast<std::uint16_t>(sign | 0x7bffu);
    if (unbiased < -14) {
        const int shift = -14 - unbiased + 13;
        if (shift >= 32) return sign;
        const auto significand = 0x800000u | mantissa;
        return static_cast<std::uint16_t>(sign | (significand >> static_cast<unsigned>(shift)));
    }
    return static_cast<std::uint16_t>(sign | (static_cast<std::uint16_t>(unbiased + 15) << 10u) | (mantissa >> 13u));
}

std::uint32_t Value(std::uint32_t tid, std::uint32_t component) {
    const auto slot = tid * 4u + component;
    if (slot < EdgeBits.size()) return EdgeBits[slot];
    const auto seed = (slot + 1u) * 0x9e3779b1u;
    const auto mixed = seed ^ (seed >> 16u);
    return (mixed & 0x807fffffu) | (((mixed >> 23u) % 31u + 110u) << 23u);
}

std::uint64_t TexelOffset(const Format& format, std::uint32_t x, std::uint32_t y) {
    const auto mip = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.value, Width, Height, 1u).at(0);
    return mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * format.components * sizeof(std::uint16_t);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, const Format& format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format.value << 20u) | (((Width - 1u) & 3u) << 30u),
            ((Width - 1u) >> 2u) | ((Height - 1u) << 14u), (format.components == 2u ? TwoChannelSwizzle : IdentitySwizzle) | (Type2D << 28u), 0u, 0u, 0u, 0u};
}

std::uint16_t Half(std::uint32_t tid, std::uint32_t component) {
    return HalfTowardZero(Value(tid, component));
}

std::uint16_t Prefill(std::uint32_t tid, std::uint32_t component) {
    const auto raw = Value(tid + 5u, component);
    auto half = HalfTowardZero(raw);
    if ((half & 0x7c00u) == 0x7c00u && (half & 0x03ffu) != 0u) half = static_cast<std::uint16_t>((half & 0x8000u) | 0x3c00u);
    return half == 0x5a5au ? 0x3c00u : half;
}

void ValidateModuleBindings(const ShaderRecompiler::RecompileResult& result) {
    bool hasSampledBinding = false;
    bool hasStorageBinding = false;
    for (const auto& binding : result.bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::GuestImages) {
            if (binding.kind == ShaderRecompiler::DescriptorKind::SampledImage) hasSampledBinding = true;
            if (binding.kind == ShaderRecompiler::DescriptorKind::StorageImage) hasStorageBinding = true;
        }
    }
    Require(hasSampledBinding, "mixed shader: missing sampled image binding metadata");
    Require(hasStorageBinding, "mixed shader: missing storage image binding metadata");

    enum class ScalarKind { Unknown, Float, Uint, Sint };
    struct ImageInfo {
        std::uint32_t sampledType = 0;
        std::uint32_t sampled = 0;
    };

    std::unordered_map<std::uint32_t, ScalarKind> scalarTypes;
    std::unordered_map<std::uint32_t, ImageInfo> imageTypes;
    std::unordered_map<std::uint32_t, std::uint32_t> arrayTypes;
    std::unordered_map<std::uint32_t, std::uint32_t> pointerTypes;
    std::unordered_map<std::uint32_t, std::uint32_t> variableTypes;
    std::unordered_map<std::uint32_t, std::uint32_t> variableBindings;

    const auto& words = result.spirv.Words();
    Require(words.size() >= 5u && words[0] == spv::MagicNumber, "mixed shader: invalid SPIR-V magic");
    for (std::size_t at = 5u; at < words.size(); ) {
        const auto word = words[at];
        const auto count = word >> 16u;
        if (count == 0u || at + count > words.size()) break;
        const auto opcode = static_cast<spv::Op>(word & 0xffffu);
        if (opcode == spv::OpTypeFloat && count >= 3u) {
            scalarTypes[words[at + 1u]] = ScalarKind::Float;
        } else if (opcode == spv::OpTypeInt && count >= 4u) {
            scalarTypes[words[at + 1u]] = words[at + 3u] == 0u ? ScalarKind::Uint : ScalarKind::Sint;
        } else if (opcode == spv::OpTypeImage && count >= 8u) {
            imageTypes[words[at + 1u]] = {words[at + 2u], words[at + 7u]};
        } else if ((opcode == spv::OpTypeArray || opcode == spv::OpTypeRuntimeArray) && count >= 3u) {
            arrayTypes[words[at + 1u]] = words[at + 2u];
        } else if (opcode == spv::OpTypePointer && count >= 4u) {
            pointerTypes[words[at + 1u]] = words[at + 3u];
        } else if (opcode == spv::OpVariable && count >= 3u) {
            variableTypes[words[at + 2u]] = words[at + 1u];
        } else if (opcode == spv::OpDecorate && count >= 4u) {
            if (words[at + 2u] == spv::DecorationBinding) {
                variableBindings[words[at + 1u]] = words[at + 3u];
            }
        }
        at += count;
    }

    bool foundStorageUint = false;
    bool foundSampledFloat = false;
    for (const auto& [varId, bindingNum] : variableBindings) {
        auto varIt = variableTypes.find(varId);
        if (varIt == variableTypes.end()) continue;
        auto ptrIt = pointerTypes.find(varIt->second);
        if (ptrIt == pointerTypes.end()) continue;
        auto targetType = ptrIt->second;
        auto arrIt = arrayTypes.find(targetType);
        if (arrIt != arrayTypes.end()) targetType = arrIt->second;
        auto imgIt = imageTypes.find(targetType);
        if (imgIt == imageTypes.end()) continue;
        const auto& img = imgIt->second;
        auto scalarIt = scalarTypes.find(img.sampledType);
        if (scalarIt == scalarTypes.end()) continue;
        if (img.sampled == 2u) {
            Require(scalarIt->second == ScalarKind::Uint, "mixed shader: storage view must be Uint");
            foundStorageUint = true;
        } else if (img.sampled == 1u) {
            Require(scalarIt->second == ScalarKind::Float, "mixed shader: sampled view must be Float");
            foundSampledFloat = true;
        }
    }
    Require(foundStorageUint, "mixed shader: no storage view with Uint scalar found in SPIR-V");
    Require(foundSampledFloat, "mixed shader: no sampled view with Float scalar found in SPIR-V");
}

std::span<const std::uint32_t> ShaderCode(bool d16, bool mixed) {
    if (mixed) return d16 ? std::span<const std::uint32_t>(MixedD16Code) : std::span<const std::uint32_t>(MixedCode);
    return d16 ? std::span<const std::uint32_t>(StoreD16Code) : std::span<const std::uint32_t>(StoreCode);
}

ShaderRecompiler::SpirvTarget Target() {
    static constexpr std::array<std::uint32_t, 8> capabilities{
        spv::CapabilityInt64, spv::CapabilityFloat64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance,
        spv::CapabilityFloat16, spv::CapabilityRoundingModeRTE, spv::CapabilityDenormPreserve,
    };
    static constexpr std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer",
    };
    ShaderRecompiler::SpirvTarget target{};
    target.vulkanVersion = 0x00401000u;
    target.spirvVersion = 0x00010300u;
    target.subgroupSize = 32;
    target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    target.supportedCapabilities = capabilities;
    target.supportedExtensions = extensions;
    target.fragmentShaderBarycentricEnabled = false;
    target.maxWorkgroupSize = {1024, 1024, 64};
    target.maxWorkgroupInvocations = 1024;
    target.maxWorkgroupSharedMemoryBytes = 32768;
    return target;
}

ShaderRecompiler::RecompileResult Recompile(const ShaderRecompiler::SpirvTarget& target, const Format& format, bool d16, bool mixed, const void* texels = nullptr) {
    const auto code = ShaderCode(d16, mixed);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * sizeof(Input[0])));
    const auto texture = TextureDescriptor(texels, format);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    return result;
}

void Store(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, bool d16, bool mixed) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            Input[tid * 4u + component] = d16 ? component < 2u ? static_cast<std::uint32_t>(Half(tid, component * 2u)) | (static_cast<std::uint32_t>(Half(tid, component * 2u + 1u)) << 16u) : 0u : Value(tid, component);
        }
    }
    const auto code = ShaderCode(d16, mixed);
    std::fill(texels, texels + TexelBytes, 0x5au);
    if (mixed) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = TexelOffset(format, x, 2u);
            for (std::uint32_t component = 0; component < format.components; ++component) {
                const auto value = Prefill(x, component);
                std::memcpy(texels + offset + component * sizeof(value), &value, sizeof(value));
            }
        }
    }
    const auto result = Recompile(device.ComputeTarget(64), format, d16, mixed, texels);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

std::uint16_t ExpectedComponent(std::uint32_t tid, std::uint32_t dmask, std::uint32_t component) {
    if (((dmask >> component) & 1u) == 0u) return 0u;
    std::uint32_t source = 0u;
    for (std::uint32_t lower = 0u; lower < component; ++lower) source += (dmask >> lower) & 1u;
    return Half(tid, source);
}

void Check(const std::uint8_t* texels, const Format& format, bool d16, bool mixed) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = TexelOffset(format, x, y);
            for (std::uint32_t component = 0; component < format.components; ++component) {
                std::uint16_t actual = 0u;
                std::memcpy(&actual, texels + offset + component * sizeof(actual), sizeof(actual));
                std::uint16_t expected = 0x5a5au;
                if (y == Writes[0].row) {
                    expected = ExpectedComponent(x, Writes[0].dmask, component);
                } else if (y == Writes[1].row) {
                    expected = ExpectedComponent(x, Writes[1].dmask, component);
                } else if (mixed && (y == 2u || y == 3u)) {
                    expected = Prefill(x, component);
                }
                Require(actual == expected, std::string(d16 ? "D16 image_store of " : "image_store of ") + format.name + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
            }
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--codegen-only") {
                const auto target = Target();
                GuestBlock block;
                for (const bool d16 : {false, true}) {
                    for (const auto& format : Formats) {
                        const auto result = Recompile(target, format, d16, true, block.Data());
                        ValidateModuleBindings(result);
                    }
                }
                std::puts("image store float rounding codegen tests passed");
                return 0;
            }
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        for (const bool d16 : {false, true}) {
            for (const auto& format : Formats) {
                for (const bool mixed : {false, true}) {
                    Store(*device, block.Data(), format, d16, mixed);
                    Check(block.Data(), format, d16, mixed);
                }
            }
        }
        std::puts("image store float rounding tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
