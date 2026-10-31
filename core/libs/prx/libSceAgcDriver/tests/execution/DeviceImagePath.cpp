#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeviceImageTable.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::DeviceImageTable;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::uint32_t Entries = 2000;
constexpr std::uint32_t RecordBytes = 32;
constexpr std::uint32_t Side = 4;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Textures = 16;
constexpr std::size_t BlockBytes = std::size_t{2} << 20u;
constexpr std::size_t TextureStride = 0x1000;
constexpr std::size_t TableOffset = 0x100000;
constexpr std::size_t SrtOffset = 0x180000;
constexpr std::array<std::uint32_t, 5> Formats{1u, 13u, 14u, 22u, 56u};
constexpr std::array<const char*, 5> FormatNames{"8 unorm", "16 float", "8_8 unorm", "32 float", "8_8_8_8 unorm"};
constexpr std::array<std::uint16_t, 4> HalfValues{0x3266u, 0x38cdu, 0x3666u, 0x3a66u};
constexpr std::array<float, 4> FloatValues{0.2f, 0.6f, 0.4f, 0.8f};

alignas(256) constexpr std::array<std::uint32_t, 18> ComputeCode{
    0x7e0002f0u, 0x7e0202f0u, 0xf4080200u, 0xfa000000u, 0xf4080300u, 0xfa000010u, 0xbf8cc07fu, 0x8f108502u, 0xf42c0504u, 0x20000000u,
    0xbf8cc07fu, 0xf09c0f08u, 0x00650000u, 0xbf8c0000u, 0xe0780000u, 0x80010000u, 0xbf810000u, 0xbf810000u,
};

alignas(4096) std::array<std::array<float, 4>, 64> Outputs{};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Texture(std::uint32_t index) const { return block + static_cast<std::size_t>(index) * TextureStride; }
    std::uint32_t* Table() const { return reinterpret_cast<std::uint32_t*>(block + TableOffset); }
    std::uint32_t* Srt() const { return reinterpret_cast<std::uint32_t*>(block + SrtOffset); }

private:
    std::uint8_t* block = nullptr;
};

float HalfToFloat(std::uint16_t half) {
    const auto exponent = static_cast<int>((half >> 10u) & 0x1fu);
    const auto mantissa = static_cast<float>(half & 0x3ffu);
    return std::ldexp(1.0f + mantissa / 1024.0f, exponent - 15);
}

struct Texel {
    std::uint32_t format = 0;
    std::vector<std::uint8_t> bytes;
    std::array<float, 4> value{};
};

Texel TexelOf(std::uint32_t texture) {
    const auto kind = texture % static_cast<std::uint32_t>(Formats.size());
    const auto variant = texture / static_cast<std::uint32_t>(Formats.size());
    Texel texel;
    texel.format = Formats[kind];
    const auto unorm = [](std::uint8_t byte) { return static_cast<float>(byte) / 255.0f; };
    switch (kind) {
        case 0: {
            const auto red = static_cast<std::uint8_t>(0x40u + 0x30u * variant);
            texel.bytes = {red};
            texel.value = {unorm(red), 0.0f, 0.0f, 1.0f};
            break;
        }
        case 1: {
            const auto half = HalfValues[variant];
            texel.bytes = {static_cast<std::uint8_t>(half), static_cast<std::uint8_t>(half >> 8u)};
            texel.value = {HalfToFloat(half), 0.0f, 0.0f, 1.0f};
            break;
        }
        case 2: {
            const auto red = static_cast<std::uint8_t>(0x11u + 0x22u * variant);
            const auto green = static_cast<std::uint8_t>(0x99u - 0x11u * variant);
            texel.bytes = {red, green};
            texel.value = {unorm(red), unorm(green), 0.0f, 1.0f};
            break;
        }
        case 3: {
            const auto bits = std::bit_cast<std::uint32_t>(FloatValues[variant]);
            texel.bytes = {static_cast<std::uint8_t>(bits), static_cast<std::uint8_t>(bits >> 8u), static_cast<std::uint8_t>(bits >> 16u), static_cast<std::uint8_t>(bits >> 24u)};
            texel.value = {FloatValues[variant], 0.0f, 0.0f, 1.0f};
            break;
        }
        default: {
            const std::array<std::uint8_t, 4> rgba{static_cast<std::uint8_t>(0x10u + 0x30u * variant), static_cast<std::uint8_t>(0x80u - 0x20u * variant), static_cast<std::uint8_t>(0xc0u - 0x30u * variant), static_cast<std::uint8_t>(0xffu - 0x40u * variant)};
            texel.bytes.assign(rgba.begin(), rgba.end());
            texel.value = {unorm(rgba[0]), unorm(rgba[1]), unorm(rgba[2]), unorm(rgba[3])};
            break;
        }
    }
    return texel;
}

void FillTexture(std::uint8_t* memory, std::span<const std::uint8_t> bytes) {
    for (std::size_t offset = 0; offset + bytes.size() <= TextureStride; offset += bytes.size()) std::memcpy(memory + offset, bytes.data(), bytes.size());
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::uint32_t TextureOf(std::uint32_t entry) {
    return entry % Textures;
}

void FillGuest(const GuestBlock& block) {
    for (std::uint32_t texture = 0; texture < Textures; ++texture) FillTexture(block.Texture(texture), TexelOf(texture).bytes);
    auto* table = block.Table();
    for (std::uint32_t entry = 0; entry < Entries; ++entry) {
        const auto texture = TextureOf(entry);
        const auto descriptor = TextureDescriptor(block.Texture(texture), TexelOf(texture).format);
        std::copy(descriptor.begin(), descriptor.end(), table + static_cast<std::size_t>(entry) * (RecordBytes / 4u));
    }
    auto* srt = block.Srt();
    const auto tableWords = BufferDescriptor(table, Entries * RecordBytes);
    std::copy(tableWords.begin(), tableWords.end(), srt);
    std::fill_n(srt + 4, 4, 0u);
}

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header ComputeHeader{};

void Registered(Header& header, const std::uint32_t* code, std::size_t bytes) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = 0;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void Dispatch(std::vector<std::uint32_t>& words, const GuestBlock& block, std::uint32_t key, std::array<float, 4>& output) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ComputeCode.data()));
    const auto srt = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block.Srt()));
    const auto set = [&](std::uint32_t offset, std::uint32_t value) { Append(words, 0x76, {offset, value}); };
    set(0x20c, static_cast<std::uint32_t>(address >> 8u));
    set(0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (const std::uint32_t offset : {0x207u, 0x208u, 0x209u}) set(offset, 1);
    set(0x212, 0);
    set(0x213, 16u << 1u);
    std::array<std::uint32_t, 16> userData{};
    userData[0] = static_cast<std::uint32_t>(srt);
    userData[1] = static_cast<std::uint32_t>(srt >> 32u);
    userData[2] = key;
    const auto outputWords = BufferDescriptor(output.data(), sizeof(output));
    std::copy(outputWords.begin(), outputWords.end(), userData.begin() + 4);
    for (std::uint32_t index = 0; index < userData.size(); ++index) set(0x240u + index, userData[index]);
    Append(words, 0x15, {1, 1, 1, 0x8041u});
}

void SubmitCompute(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "the compute submission failed");
    AgcDriverWaitIdle_nid_postfix();
    std::vector<std::byte> stored(sizeof(Outputs));
    AgcDriver::GuestMemory::Read(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Outputs.data())), stored, 4);
    std::memcpy(Outputs.data(), stored.data(), stored.size());
}

struct Counters {
    std::uint64_t heapComputeStages = 0;
    std::uint64_t tables = 0;
    DeviceImageTable::Counts device;
};

Counters Read() {
    Counters counters;
    counters.heapComputeStages = AgcDriver::Graphics::HeapComputeStages();
    const auto totals = DeviceImageTable::DeviceTotals();
    counters.tables = totals.size();
    for (const auto& table : totals) {
        counters.device.prepares += table.prepares;
        counters.device.drained += table.drained;
        counters.device.inserted += table.inserted;
        counters.device.expected += table.expected;
        counters.device.expectedInserted += table.expectedInserted;
        counters.device.faults += table.faults;
        counters.device.rejected += table.rejected;
        counters.device.fallbacks += table.fallbacks;
    }
    return counters;
}

std::string Describe(const Counters& before, const Counters& after) {
    return " (GPU-mode compute stages " + std::to_string(after.heapComputeStages - before.heapComputeStages) + ", device table prepares " + std::to_string(after.device.prepares - before.device.prepares) + ", expected " + std::to_string(after.device.expected - before.device.expected) + ", inserted " + std::to_string(after.device.inserted - before.device.inserted) + ", miss records " + std::to_string(after.device.drained - before.device.drained) + ", fallbacks " + std::to_string(after.device.fallbacks - before.device.fallbacks) + ")";
}

void RequireTexel(const std::array<float, 4>& output, const std::array<float, 4>& expected, const std::string& what) {
    for (std::size_t channel = 0; channel < 4; ++channel) {
        Require(std::fabs(output[channel] - expected[channel]) < 1e-6f, what + ": channel " + std::to_string(channel) + " is " + std::to_string(output[channel]) + ", expected " + std::to_string(expected[channel]));
    }
}

constexpr std::array<std::uint32_t, 10> ComputeKeys{3u, 1764u, 37u, 54u, 1495u, 72u, 1657u, 1600u, 33u, 1810u};

void RunComputeKeys(const GuestBlock& block) {
    for (std::size_t index = 0; index < ComputeKeys.size(); ++index) Outputs[index].fill(-1.0f);
    std::vector<std::uint32_t> words;
    for (std::size_t index = 0; index < ComputeKeys.size(); ++index) Dispatch(words, block, ComputeKeys[index], Outputs[index]);
    SubmitCompute(words);
}

void RequireComputeTexels(const std::string& what) {
    for (std::size_t index = 0; index < ComputeKeys.size(); ++index) {
        const auto texture = TextureOf(ComputeKeys[index]);
        RequireTexel(Outputs[index], TexelOf(texture).value, what + ", key " + std::to_string(ComputeKeys[index]) + " (" + FormatNames[texture % Formats.size()] + ")");
    }
}

void ComputeTable(const GuestBlock& block, bool device) {
    auto before = Read();
    RunComputeKeys(block);
    auto after = Read();
    RequireComputeTexels("first batch");
    if (device) {
        Require(after.tables == 1u, "the device keeps one device image table" + Describe(before, after));
        Require(after.heapComputeStages > before.heapComputeStages, "first batch: the sampling compute stage is not GPU mode" + Describe(before, after));
        Require(after.device.prepares > before.device.prepares, "first batch: the GPU-mode stage did not read its table through the device image table" + Describe(before, after));
        Require(after.device.expectedInserted == before.device.expectedInserted + ComputeKeys.size(), "first batch: the driver did not insert the ten expected keys before the batch" + Describe(before, after));
        Require(after.device.drained == before.device.drained, "first batch: the device path recorded misses for keys it expected" + Describe(before, after));
        Require(after.device.fallbacks == before.device.fallbacks, "first batch: the program fell back to the CPU table map" + Describe(before, after));
    } else {
        Require(after.tables == 0u, "without APS5_GPU_PATH the device keeps no device image table");
    }
    std::printf("first batch of %zu dispatches: %s%s\n", ComputeKeys.size(), device ? "texels through the device image table, keys inserted before the batch" : "texels through the CPU table map", Describe(before, after).c_str());
    before = after;
    RunComputeKeys(block);
    after = Read();
    RequireComputeTexels("second batch");
    if (device) {
        Require(after.device.inserted == before.device.inserted, "second batch: the device path inserted keys it already held" + Describe(before, after));
        Require(after.device.drained == before.device.drained, "second batch: the device path recorded misses for keys it held" + Describe(before, after));
    }
    std::printf("second batch: the same texels%s\n", Describe(before, after).c_str());
}

}

int main() {
    const bool requested = AgcDriver::Graphics::GpuPathEnabled();
#ifdef _WIN32
    _putenv_s("APS5_EAGER_WRITEBACK", "1");
#else
    setenv("APS5_EAGER_WRITEBACK", "1", 1);
#endif
    try {
        bool served = false;
        bool heapUsable = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            served = device->DeviceImagePath();
            heapUsable = AgcDriver::Graphics::DescriptorHeapEnabled();
            Require(device->Target().deviceImageTables == served && device->ComputeTarget(64u).deviceImageTables == served, "the recompile targets ask for device image tables exactly when the device serves the path");
            Require((DeviceImageTable::Existing(device->GraphicsContext()) != nullptr) == served, "the device keeps a device image table exactly when it serves the path");
        }
        const bool gpuHeap = AgcDriver::Graphics::GpuHeapEnabled();
        Require(served == (requested && gpuHeap && heapUsable), "the device serves the device image path exactly when APS5_GPU_PATH and APS5_GPU_HEAP are set on a device whose descriptor heap the GPU heap uses");
        if (requested && gpuHeap && !served) {
            std::printf("skipped, the device lacks VK_EXT_descriptor_heap, which the GPU heap the device image path reads its tables through needs\n");
            return VulkanTestSkipped;
        }
        GuestBlock block;
        FillGuest(block);
        Registered(ComputeHeader, ComputeCode.data(), sizeof(ComputeCode));
        std::printf("device image path %s\n", served ? "on" : "off");
        ComputeTable(block, served);
        AgcDriverShutdown_nid_postfix();
        std::cout << "device image path tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "device image path tests failed: " << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
