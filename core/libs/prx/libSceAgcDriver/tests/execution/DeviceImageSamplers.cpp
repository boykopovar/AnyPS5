#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeviceImageTable.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::DeviceImageTable;
using SamplerWords = std::array<std::uint32_t, 4>;
using Texel = std::array<float, 4>;
namespace Abi = ShaderRecompiler::DeviceImageTableAbi;

constexpr std::size_t BlockBytes = 0x10000;
constexpr std::size_t ImagesOffset = 0x1000;
constexpr std::size_t SamplersOffset = 0x2000;
constexpr std::size_t OtherSamplersOffset = 0x3000;
constexpr std::size_t SrtOffset = 0x4000;
constexpr std::size_t OutputsOffset = 0x5000;
constexpr std::size_t SamplerPointerOffset = 0x6000;
constexpr std::uint32_t ImageEntries = 2;
constexpr std::uint32_t SamplerEntries = 2;
constexpr SamplerWords PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};
constexpr SamplerWords PointRepeat{0u, (4u * 256u) << 12u, 0u, 0u};
constexpr Texel ClampTexel{1.0f, 0.0f, 0.0f, 1.0f};
constexpr Texel RepeatTexel{0.0f, 1.0f, 0.0f, 1.0f};

alignas(256) constexpr std::array<std::uint32_t, 22> TableCode{
    0x7e0002ffu, 0x3f900000u, 0x7e0202ffu, 0x3e000000u,
    0xf4080200u, 0xfa000000u, 0xf4080300u, 0xfa000010u, 0xbf8cc07fu,
    0x8f108502u, 0x8f118403u, 0xf42c0504u, 0x20000000u, 0xf4280706u, 0x22000000u,
    0xbf8cc07fu, 0xf09c0f08u, 0x00e50000u, 0xbf8c0000u, 0xe0780000u, 0x80010000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 15> DirectCode{
    0x7e0002ffu, 0x3f900000u, 0x7e0202ffu, 0x3e000000u,
    0xf40c0500u, 0xfa000020u, 0xf4080700u, 0xfa000040u, 0xbf8cc07fu,
    0xf09c0f08u, 0x00e50000u, 0xbf8c0000u, 0xe0780000u, 0x80010000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 26> RuntimeAddressCode{
    0x7e0002ffu, 0x3f900000u, 0x7e0202ffu, 0x3e000000u,
    0xf4080200u, 0xfa000000u, 0xf4080300u, 0xfa000010u, 0xbf8cc07fu,
    0x8f108502u, 0xf42c0504u, 0x20000000u, 0xe0340000u, 0x80030200u, 0xbf8c0000u,
    0x7e240502u, 0x7e260503u, 0xf4080709u, 0xfa000000u, 0xbf8cc07fu,
    0xf09c0f08u, 0x00e50000u, 0xbf8c0000u, 0xe0780000u, 0x80010000u, 0xbf810000u,
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        void* mapped = mmap(nullptr, BlockBytes * 2u, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + BlockBytes - 1u) & ~(BlockBytes - 1u);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes * 2u) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + BlockBytes - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "cannot allocate the sampler test guest block");
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

    std::uint8_t* Texture() const { return block; }
    std::uint32_t* Images() const { return reinterpret_cast<std::uint32_t*>(block + ImagesOffset); }
    std::uint32_t* Samplers() const { return reinterpret_cast<std::uint32_t*>(block + SamplersOffset); }
    std::uint32_t* OtherSamplers() const { return reinterpret_cast<std::uint32_t*>(block + OtherSamplersOffset); }
    std::uint32_t* Srt() const { return reinterpret_cast<std::uint32_t*>(block + SrtOffset); }
    float* Output(std::size_t index) const { return reinterpret_cast<float*>(block + OutputsOffset) + index * 4u; }
    std::uint32_t* SamplerPointer() const { return reinterpret_cast<std::uint32_t*>(block + SamplerPointerOffset); }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> Image(const void* pointer) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    return {static_cast<std::uint32_t>(address >> 8u),
            static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (3u << 30u),
            3u << 14u, 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
}

std::array<std::uint32_t, 4> Buffer(const void* pointer, std::uint32_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void FillGuest(const GuestBlock& block) {
    constexpr std::array<std::array<std::uint8_t, 4>, 4> colors{{{0u, 255u, 0u, 255u}, {0u, 0u, 255u, 255u}, {255u, 255u, 0u, 255u}, {255u, 0u, 0u, 255u}}};
    for (std::size_t y = 0; y < 4u; ++y) {
        for (std::size_t x = 0; x < 4u; ++x) std::memcpy(block.Texture() + (y * 4u + x) * 4u, colors[x].data(), 4u);
    }
    const auto image = Image(block.Texture());
    for (std::size_t entry = 0; entry < ImageEntries; ++entry) std::copy(image.begin(), image.end(), block.Images() + entry * 8u);
    std::copy(PointClamp.begin(), PointClamp.end(), block.Samplers());
    std::copy(PointRepeat.begin(), PointRepeat.end(), block.Samplers() + 4u);
    std::copy(PointClamp.begin(), PointClamp.end(), block.OtherSamplers());
    std::copy(PointRepeat.begin(), PointRepeat.end(), block.OtherSamplers() + 4u);
    const auto images = Buffer(block.Images(), ImageEntries * 32u);
    const auto samplers = Buffer(block.Samplers(), SamplerEntries * 16u);
    std::copy(images.begin(), images.end(), block.Srt());
    std::copy(samplers.begin(), samplers.end(), block.Srt() + 4u);
    std::copy(image.begin(), image.end(), block.Srt() + 8u);
}

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header TableHeader{};
alignas(8) Header DirectHeader{};
alignas(8) Header RuntimeAddressHeader{};

void Register(Header& header, std::span<const std::uint32_t> code) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(code.size_bytes());
    header.shader.code = code.data();
    header.shader.type = 0;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1u) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

Texel Sample(const GuestBlock& block, const std::uint32_t* code, std::uint32_t imageIndex, std::uint32_t samplerIndex, std::size_t outputIndex) {
    auto* output = block.Output(outputIndex);
    std::fill_n(output, 4u, -1.0f);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(code));
    const auto srt = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block.Srt()));
    std::vector<std::uint32_t> commands;
    const auto set = [&](std::uint32_t offset, std::uint32_t value) { Append(commands, 0x76u, {offset, value}); };
    set(0x20cu, static_cast<std::uint32_t>(address >> 8u));
    set(0x20du, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (const auto offset : {0x207u, 0x208u, 0x209u}) set(offset, 1u);
    set(0x212u, 0u);
    set(0x213u, 16u << 1u);
    std::array<std::uint32_t, 16> userData{};
    userData[0] = static_cast<std::uint32_t>(srt);
    userData[1] = static_cast<std::uint32_t>(srt >> 32u);
    userData[2] = imageIndex;
    userData[3] = samplerIndex;
    const auto outputBuffer = Buffer(output, sizeof(Texel));
    std::copy(outputBuffer.begin(), outputBuffer.end(), userData.begin() + 4u);
    for (std::uint32_t index = 0; index < userData.size(); ++index) set(0x240u + index, userData[index]);
    Append(commands, 0x15u, {1u, 1u, 1u, 0x8041u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0u, {}};
    Require(sceAgcDriverSubmitAcb(0x20u, &packet) == 0, "the sampler test compute submission failed");
    AgcDriverWaitIdle_nid_postfix();
    Texel stored{};
    AgcDriver::GuestMemory::Read(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output)), std::as_writable_bytes(std::span(stored)), 4u);
    return stored;
}

struct Counts {
    std::uint64_t heapBinds = 0;
    std::uint64_t tables = 0;
    std::uint64_t prepares = 0;
    std::uint64_t expectedInserted = 0;
    std::uint64_t inserted = 0;
    std::uint64_t entries = 0;
    std::uint64_t misses = 0;
    std::uint64_t fallbacks = 0;
    std::uint64_t faults = 0;
};

Counts ReadCounts() {
    Counts result;
    result.heapBinds = AgcDriver::Graphics::DescriptorHeapBinds();
    const auto tables = DeviceImageTable::DeviceTotals();
    result.tables = tables.size();
    for (const auto& table : tables) {
        result.prepares += table.prepares;
        result.expectedInserted += table.expectedInserted;
        result.inserted += table.inserted;
        result.entries += table.entries;
        result.misses += table.drained;
        result.fallbacks += table.fallbacks;
        result.faults += table.faults;
    }
    return result;
}

std::string Describe(const Counts& before, const Counts& after) {
    return " (heap binds " + std::to_string(after.heapBinds - before.heapBinds) + ", prepares " + std::to_string(after.prepares - before.prepares) +
           ", expected inserts " + std::to_string(after.expectedInserted - before.expectedInserted) + ", inserts " + std::to_string(after.inserted - before.inserted) + ", misses " + std::to_string(after.misses - before.misses) +
           ", fallbacks " + std::to_string(after.fallbacks - before.fallbacks) + ", faults " + std::to_string(after.faults - before.faults) + ")";
}

void RequireTexel(const Texel& actual, const Texel& expected, const std::string& what) {
    for (std::size_t channel = 0; channel < actual.size(); ++channel) {
        Require(std::fabs(actual[channel] - expected[channel]) < 1e-6f,
                what + ": channel " + std::to_string(channel) + " is " + std::to_string(actual[channel]) + ", expected " + std::to_string(expected[channel]));
    }
}

void CheckSamplers(const GuestBlock& block, bool devicePath) {
    std::copy(PointClamp.begin(), PointClamp.end(), block.Srt() + 16u);
    const auto directClamp = Sample(block, DirectCode.data(), 0u, 0u, 0u);
    std::copy(PointRepeat.begin(), PointRepeat.end(), block.Srt() + 16u);
    const auto directRepeat = Sample(block, DirectCode.data(), 0u, 0u, 1u);
    RequireTexel(directClamp, ClampTexel, "direct clamp-to-edge control");
    RequireTexel(directRepeat, RepeatTexel, "direct repeat control");
    Require(directClamp != directRepeat, "direct controls did not distinguish sampler addressing modes");

    const auto before = ReadCounts();
    RequireTexel(Sample(block, TableCode.data(), 1u, 0u, 2u), directClamp, "image 1 with sampler 0");
    RequireTexel(Sample(block, TableCode.data(), 1u, 1u, 3u), directRepeat, "image 1 with sampler 1");
    RequireTexel(Sample(block, TableCode.data(), 0u, 1u, 4u), directRepeat, "image 0 with independent sampler 1");
    const auto chosen = ReadCounts();
    if (devicePath) {
        const auto detail = Describe(before, chosen);
        Require(chosen.tables == 1u && chosen.heapBinds >= before.heapBinds + 3u, "sampler choices did not execute as GPU-mode stages" + detail);
        Require(chosen.prepares >= before.prepares + 3u && chosen.expectedInserted >= before.expectedInserted + 2u, "different sampler words did not produce device-table entries" + detail);
        Require(chosen.misses == before.misses && chosen.fallbacks == before.fallbacks && chosen.faults == before.faults, "expected sampler choices missed, faulted or fell back" + detail);
    } else {
        Require(chosen.tables == 0u, "the CPU sampler path created a device image table");
    }

    auto previous = chosen;
    const auto checkMutation = [&] {
        const auto current = ReadCounts();
        if (devicePath) {
            const auto detail = Describe(previous, current);
            Require(current.tables == 1u && current.heapBinds > previous.heapBinds && current.prepares > previous.prepares, "mutated sampler choices left the device path" + detail);
            Require(current.misses == previous.misses && current.fallbacks == previous.fallbacks && current.faults == previous.faults, "mutated sampler choices missed, faulted or fell back" + detail);
        }
        previous = current;
    };
    std::copy(PointRepeat.begin(), PointRepeat.end(), block.Samplers());
    RequireTexel(Sample(block, TableCode.data(), 1u, 0u, 2u), directRepeat, "changing only the selected S# words");
    checkMutation();
    const auto moved = Buffer(block.OtherSamplers(), SamplerEntries * 16u);
    std::copy(moved.begin(), moved.end(), block.Srt() + 4u);
    RequireTexel(Sample(block, TableCode.data(), 1u, 0u, 2u), directClamp, "moving only the S# table pointer");
    checkMutation();
    RequireTexel(Sample(block, TableCode.data(), 1u, 1u, 3u), directRepeat, "sampler 1 after moving the S# table pointer");
    checkMutation();
    const auto after = previous;
    std::printf("table sampler clamp/repeat, independent indices, S# mutation and moved sampler table: correct texels through %s%s\n",
                devicePath ? "the device table" : "the CPU table map", Describe(before, after).c_str());
}

std::vector<DeviceImageTable::Key> ResolvedKeys() {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "runtime sampler keys have no active recorder");
    AgcDriver::Graphics::Context context{};
    context.device = recorder->Device();
    const auto* table = DeviceImageTable::Existing(context);
    Require(table != nullptr, "runtime sampler keys have no device image table");
    const auto words = table->KeyWords();
    Require(words.size() % Abi::BucketWords == 0u, "the runtime sampler key table has a partial bucket");
    std::vector<DeviceImageTable::Key> keys;
    for (std::size_t offset = 0; offset < words.size(); offset += Abi::BucketWords) {
        if (words[offset + Abi::StateWord] != Abi::LiveState) continue;
        DeviceImageTable::Key key{};
        std::copy_n(words.begin() + offset, Abi::KeyWords, key.begin());
        keys.push_back(key);
    }
    return keys;
}

void SetSamplerPointer(const GuestBlock& block, const std::uint32_t* sampler) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(sampler));
    block.SamplerPointer()[0] = static_cast<std::uint32_t>(address);
    block.SamplerPointer()[1] = static_cast<std::uint32_t>(address >> 32u);
}

void CheckRuntimeSamplers(const GuestBlock& block) {
    Require(DeviceImageTable::KeepsUnexpectedPrograms(), "runtime sampler addresses need APS5_KEEP_UNEXPECTED_DEVICE_IMAGES");
    std::copy(PointClamp.begin(), PointClamp.end(), block.Srt() + 16u);
    const auto directClamp = Sample(block, DirectCode.data(), 0u, 0u, 0u);
    std::copy(PointRepeat.begin(), PointRepeat.end(), block.Srt() + 16u);
    const auto directRepeat = Sample(block, DirectCode.data(), 0u, 0u, 1u);
    RequireTexel(directClamp, ClampTexel, "runtime-address direct clamp-to-edge control");
    RequireTexel(directRepeat, RepeatTexel, "runtime-address direct repeat control");
    Require(directClamp != directRepeat, "runtime-address direct controls did not distinguish sampler addressing modes");
    const auto pointerBuffer = Buffer(block.SamplerPointer(), 2u * sizeof(std::uint32_t));
    std::copy(pointerBuffer.begin(), pointerBuffer.end(), block.Srt() + 4u);
    const auto initial = ReadCounts();
    Require(initial.entries == 0u, "runtime sampler addresses require a table with no warmed image keys");
    std::size_t output = 2u;
    const auto sample = [&](const Texel& expected, bool cold, bool resolves, const std::string& what) {
        const auto before = ReadCounts();
        const auto actual = Sample(block, RuntimeAddressCode.data(), 1u, 0u, output++);
        const auto after = ReadCounts();
        const auto detail = Describe(before, after);
        Require(after.tables == 1u && after.heapBinds > before.heapBinds && after.prepares > before.prepares, what + " did not bind the descriptor heap and prepare its device image table" + detail);
        Require(after.expectedInserted == before.expectedInserted, what + " inserted a host expectation for a GPU-loaded sampler address" + detail);
        Require(after.misses == before.misses + (cold ? 1u : 0u) && after.inserted == before.inserted + (resolves ? 1u : 0u), what + " did not record or resolve exactly its own new sampler key" + detail);
        Require(after.fallbacks == before.fallbacks && after.faults == before.faults, what + " faulted or fell back" + detail);
        RequireTexel(actual, expected, what);
    };
    SetSamplerPointer(block, block.Samplers());
    sample({}, true, false, "cold runtime clamp address");
    sample(directClamp, false, true, "settled runtime clamp address");
    SetSamplerPointer(block, block.Samplers() + 4u);
    sample({}, true, false, "cold runtime repeat address");
    sample(directRepeat, false, true, "settled runtime repeat address");
    const auto keys = ResolvedKeys();
    Require(keys.size() == 2u && std::equal(keys[0].begin(), keys[0].begin() + Abi::SamplerWord, keys[1].begin()), "the two settled runtime keys differ in their image, interface or sampler class");
    const auto descriptor = Image(block.Texture());
    for (const auto& key : keys) {
        Require(std::equal(descriptor.begin(), descriptor.end(), key.begin()) && (key[Abi::SamplerClassWord] & Abi::RuntimeSampler) != 0u, "a settled runtime sampler key changed the fixed image or lacks the runtime sampler class");
    }
    const auto hasSampler = [&](const SamplerWords& words) {
        return std::any_of(keys.begin(), keys.end(), [&](const auto& key) { return std::equal(words.begin(), words.end(), key.begin() + Abi::SamplerWord); });
    };
    Require(hasSampler(PointClamp) && hasSampler(PointRepeat), "the two settled runtime keys do not contain the four raw clamp and repeat S# words");
    SetSamplerPointer(block, block.Samplers());
    sample(directClamp, false, false, "runtime address A before mutation");
    std::copy(PointRepeat.begin(), PointRepeat.end(), block.Samplers());
    sample(directRepeat, false, false, "runtime address A after changing only its S# words to B");
    SetSamplerPointer(block, block.OtherSamplers());
    sample(directClamp, false, false, "runtime pointer relocated to a separate A descriptor");
    const auto after = ReadCounts();
    Require(after.entries == 2u && after.inserted == initial.inserted + 2u && after.misses == initial.misses + 2u, "runtime sampler mutation or pointer relocation introduced extra keys or misses" + Describe(initial, after));
    std::printf("runtime sampler addresses: two cold keys, exact settled clamp/repeat and A/B/A mutation colors%s\n", Describe(initial, after).c_str());
}

}

int main(int argc, char** argv) {
    const bool runtimeAddress = argc > 1 && std::string(argv[1]) == "--runtime-address";
#ifdef _WIN32
    _putenv_s("APS5_EAGER_WRITEBACK", "1");
#else
    setenv("APS5_EAGER_WRITEBACK", "1", 1);
#endif
    try {
        bool devicePath = false;
        bool heapUsable = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            devicePath = device->DeviceImagePath();
            heapUsable = AgcDriver::Graphics::DescriptorHeapEnabled();
            Require(device->ComputeTarget(64u).deviceImageTables == devicePath, "the compute target disagrees with device-table availability");
        }
        const bool requested = AgcDriver::Graphics::GpuPathEnabled();
        const bool heapRequested = AgcDriver::Graphics::GpuHeapEnabled();
        Require(devicePath == (requested && heapRequested && heapUsable), "the device image path disagrees with its requested and supported capabilities");
        if (requested && heapRequested && !devicePath) {
            std::puts("skipped, descriptor heap features, limits or validation policy do not permit the device image path; see the device diagnostics");
            return VulkanTestSkipped;
        }
        if (runtimeAddress && !devicePath) {
            std::puts("skipped, runtime sampler addresses require the enabled and supported device image path");
            return VulkanTestSkipped;
        }
        GuestBlock block;
        FillGuest(block);
        Register(DirectHeader, DirectCode);
        if (runtimeAddress) {
            Register(RuntimeAddressHeader, RuntimeAddressCode);
            CheckRuntimeSamplers(block);
        } else {
            Register(TableHeader, TableCode);
            CheckSamplers(block, devicePath);
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("device image sampler tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "device image sampler tests failed: " << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
