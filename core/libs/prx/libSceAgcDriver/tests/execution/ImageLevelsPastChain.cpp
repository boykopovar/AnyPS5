#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t Width = 28;
constexpr std::uint32_t Layers = 4;
constexpr std::uint32_t GuestLevels = 10;
constexpr std::uint32_t ChainLevels = 5;
constexpr std::uint32_t Result = 8;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileLinear = 0;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 65536> Texels{};

constexpr std::uint32_t Prologue[] = {0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c, 0x80000501, 0xbf8c3f70};
constexpr std::uint32_t Epilogue[] = {0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000};

constexpr std::array<std::uint32_t, 22> Program(std::uint32_t first, std::uint32_t second) {
    std::array<std::uint32_t, 22> code{};
    std::size_t index = 0;
    for (const auto word : Prologue) code[index++] = word;
    code[index++] = first;
    code[index++] = second;
    for (const auto word : Epilogue) code[index++] = word;
    return code;
}

alignas(256) constexpr auto Resinfo2D = Program(0xf0381f08, 0x00010c02);
alignas(256) constexpr auto Resinfo2DArray = Program(0xf0381f28, 0x00010c02);
alignas(256) constexpr auto SampleLod2D = Program(0xf0900f08, 0x00610c02);
alignas(256) constexpr auto Load2D = Program(0xf0001f08, 0x00010c02);
alignas(256) constexpr auto LoadMip2D = Program(0xf0041f08, 0x00010c02);

alignas(256) constexpr std::array<std::uint32_t, 5> StoreCode{0x7e040300, 0x7e060280, 0xf0200108, 0x00020002, 0xbf810000};

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t LevelWidth(std::uint32_t level) {
    return std::max(1u, Width >> level);
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t tile, std::uint32_t type, std::uint32_t layers, std::uint32_t base, std::uint32_t last) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        0xfacu | (base << 12u) | (last << 16u) | (tile << 20u) | (type << 28u),
        layers - 1u,
        (GuestLevels - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 8> SampledDescriptor(std::uint32_t type, std::uint32_t layers, std::uint32_t base) {
    return TextureDescriptor(Texels.data(), Format8888UNorm, TileLinear, type, layers, base, GuestLevels - 1u);
}

std::array<std::uint8_t, 4> TexelOf(std::uint32_t level, std::uint32_t x) {
    return {static_cast<std::uint8_t>(10u + level * 20u), static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(200u - level), 255u};
}

void FillTexture() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(SampledDescriptor(Type2D, 1u, 0u)));
    Require(geometry.guestBytes <= Texels.size() && geometry.mips.size() == GuestLevels, "levels past the chain: the 28x1 surface of 10 levels does not fit the texel storage");
    Texels.fill(0xeeu);
    for (std::uint32_t level = 0; level < GuestLevels; ++level) {
        const auto& mip = geometry.mips[level];
        Require(mip.width == LevelWidth(level) && mip.height == 1u, "levels past the chain: level " + std::to_string(level) + " of the guest layout is not max(1, 28 >> level) x 1");
        for (std::uint32_t x = 0; x < mip.width; ++x) {
            const auto texel = TexelOf(level, x);
            std::copy(texel.begin(), texel.end(), &Texels[mip.tiledOffset + x * 4u]);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, std::uint32_t threads) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

template<typename TInput>
void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture, TInput input) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 4> words = input(tid);
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    Dispatch(device, code, userData, Threads);
}

std::uint32_t ResultWord(std::uint32_t tid, std::uint32_t component) {
    return Buffer[tid * Words + Result + component];
}

void CheckResinfoAt(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t type, std::uint32_t layers, std::uint32_t base, const std::function<std::uint32_t(std::uint32_t)>& lodOf) {
    const auto levels = GuestLevels - base;
    Run(device, code, SampledDescriptor(type, layers, base), [&](std::uint32_t tid) { return std::array<std::uint32_t, 4>{lodOf(tid), 0u, 0u, 0u}; });
    const std::string name = std::string("image_get_resinfo of a 28x1 ") + (type == Type2DArray ? "2D-array" : "2D") + " T# of levels " + std::to_string(base) + "-9";
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lod = lodOf(tid);
        const auto check = [&](std::uint32_t component, std::uint32_t expected, const char* what) {
            const auto value = ResultWord(tid, component);
            Require(value == expected, name + " at LOD " + std::to_string(lod) + ": " + what + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
        };
        check(0u, LevelWidth(base + lod), "the width");
        check(1u, 1u, "the height");
        if (type == Type2DArray) check(2u, layers, "the layer count");
        check(3u, levels, "the level count");
    }
}

void CheckResinfo(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t type, std::uint32_t layers, std::uint32_t base) {
    const auto levels = GuestLevels - base;
    for (std::uint32_t lod = 0; lod < levels; ++lod) CheckResinfoAt(device, code, type, layers, base, [lod](std::uint32_t) { return lod; });
    if (device.DeviceName().starts_with("llvmpipe")) {
        std::puts("skipped the image_get_resinfo dispatches whose LOD differs between invocations: llvmpipe before Mesa 25.3 answers a size query of a vector of invocations at the LOD of its first");
        return;
    }
    CheckResinfoAt(device, code, type, layers, base, [levels](std::uint32_t tid) { return tid % levels; });
}

void CheckSampledLevels(AgcDriver::VulkanDevice& device) {
    const auto columnOf = [](std::uint32_t tid) { return (tid * 7u + 3u) % Width; };
    Run(device, SampleLod2D, SampledDescriptor(Type2D, 1u, 0u), [&](std::uint32_t tid) {
        return std::array<std::uint32_t, 4>{Bits((static_cast<float>(columnOf(tid)) + 0.5f) / static_cast<float>(Width)), Bits(0.5f), Bits(static_cast<float>(tid % ChainLevels)), 0u};
    });
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto level = tid % ChainLevels;
        const auto width = LevelWidth(level);
        const auto column = std::min((2u * columnOf(tid) + 1u) * width / (2u * Width), width - 1u);
        const auto texel = TexelOf(level, column);
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto value = std::bit_cast<float>(ResultWord(tid, component)) * 255.0f;
            Require(std::lround(value) == static_cast<long>(texel[component]), "image_sample_l of level " + std::to_string(level) + " of a 28x1 T# of 10 levels: column " + std::to_string(columnOf(tid)) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(texel[component]));
        }
    }
}

void CheckBaseLoad(AgcDriver::VulkanDevice& device) {
    Run(device, Load2D, SampledDescriptor(Type2D, 1u, 0u), [](std::uint32_t tid) { return std::array<std::uint32_t, 4>{tid % Width, 0u, 0u, 0u}; });
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto texel = TexelOf(0u, tid % Width);
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto value = std::bit_cast<float>(ResultWord(tid, component)) * 255.0f;
            Require(std::lround(value) == static_cast<long>(texel[component]), "image_load at base level 0 of a 28x1 T# of 10 levels: x " + std::to_string(tid % Width) + " component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(texel[component]));
        }
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t base, const std::string& name) {
    std::string refusal;
    try {
        Run(device, code, SampledDescriptor(Type2D, 1u, base), [](std::uint32_t tid) { return std::array<std::uint32_t, 4>{0u, 0u, tid % GuestLevels, 0u}; });
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("past its extent's full mip chain") != std::string::npos, name + " of a 28x1 T# of 10 levels was not refused: " + (refusal.empty() ? std::string("it ran") : refusal));
}

void CheckStorageWriteBack(AgcDriver::VulkanDevice& device, std::uint32_t tile) {
    const auto descriptor = [&](const void* data) { return TextureDescriptor(data, Format32UInt, tile, Type2D, 1u, 0u, 0u); };
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor(Texels.data())));
    const auto surfaceBytes = (static_cast<std::size_t>(geometry.guestBytes) + 0xffffu) & ~std::size_t{0xffffu};
    static std::array<std::vector<std::uint32_t>, 2> storages;
    auto& storage = storages[tile == TileLinear ? 0u : 1u];
    storage.assign((surfaceBytes + 0x10000u) / 4u, 0u);
    auto* texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
    std::fill(texels, texels + surfaceBytes / 4u, Untouched);
    struct Registration {
        void* data;
        Registration(void* data, std::size_t bytes) : data(data) {
            GuestAllocations::Mutation mutation;
            mutation.Add(data, bytes, true, true, true);
        }
        ~Registration() {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
        Registration(const Registration&) = delete;
        Registration& operator=(const Registration&) = delete;
    } registration(texels, surfaceBytes);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = descriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    Dispatch(device, StoreCode, userData, Width);
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Graphics::FlushCachedTextures(device.Device());
    }
    device.WaitIdle();
    const std::string name = std::string("image_store to level 0 of a 28x1 ") + (tile == TileLinear ? "linear" : "R64KBX") + " R32_UINT surface of 10 levels";
    std::vector<std::uint32_t> stored;
    for (std::size_t index = 0; index < surfaceBytes / 4u; ++index) {
        if (texels[index] != Untouched) stored.push_back(texels[index]);
    }
    std::ranges::sort(stored);
    std::vector<std::uint32_t> expected(Width);
    for (std::uint32_t x = 0; x < Width; ++x) expected[x] = x;
    Require(stored == expected, name + ": " + std::to_string(stored.size()) + " dwords of the surface changed, expected the 28 texels of level 0 and no byte of levels 1-9");
    if (tile == TileLinear) {
        const auto* level = texels + geometry.mips.at(0).tiledOffset / 4u;
        for (std::uint32_t x = 0; x < Width; ++x) Require(level[x] == x, name + ": texel " + std::to_string(x) + " of level 0 holds " + std::to_string(level[x]));
    }
}

}

int main() {
    std::unique_ptr<AgcDriver::VulkanDevice> device;
    try {
        device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    auto& gpu = *device;
    const std::array<std::function<void()>, 9> checks{
        [&] { CheckSampledLevels(gpu); },
        [&] { CheckBaseLoad(gpu); },
        [&] { CheckResinfo(gpu, Resinfo2D, Type2D, 1u, 0u); },
        [&] { CheckResinfo(gpu, Resinfo2DArray, Type2DArray, Layers, 0u); },
        [&] { CheckResinfo(gpu, Resinfo2D, Type2D, 1u, 7u); },
        [&] { RequireRefused(gpu, LoadMip2D, 0u, "image_load_mip"); },
        [&] { RequireRefused(gpu, Load2D, 7u, "image_load at base level 7"); },
        [&] { CheckStorageWriteBack(gpu, TileLinear); },
        [&] { CheckStorageWriteBack(gpu, TileR64KBX); },
    };
    int failures = 0;
    for (const auto& check : checks) {
        try {
            check();
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            ++failures;
        }
    }
    if (failures != 0) return 1;
    std::puts("levels past the full chain tests passed");
    return 0;
}
