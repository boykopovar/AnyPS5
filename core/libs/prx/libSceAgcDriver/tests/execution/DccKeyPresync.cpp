#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t TileRenderTarget64KB = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = 65536;
constexpr std::size_t KeyBytes = SurfaceBytes / 256;
constexpr std::size_t KeysOffset = SurfaceBytes;
constexpr std::size_t SourceKeysOffset = SurfaceBytes + 4096;
constexpr std::size_t BlockBytes = 2 * 65536;

alignas(256) constexpr std::array<std::uint32_t, 7> CopyKeysCode{
    0x34020083, 0xe0341000, 0x80000201, 0xbf8c3f70, 0xe0741000, 0x80010201, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> SampleSecondTexelCode{
    0x34060084, 0x7e2802ff, 0x3c400000, 0x7e2a02ff, 0x3b800000, 0xf09c0f08, 0x00610a14, 0xbf8c3f70,
    0xe0781000, 0x80000a03, 0xbf810000,
};

constexpr std::array<std::uint32_t, 4> PointSampler{0x00000092u, 0x00fff000u, 0u, 0u};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "DCC key pre-sync: cannot allocate the guest block");
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

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels, const std::uint8_t* keys) {
    const auto address = AddressOf(texels);
    const auto meta = AddressOf(keys) >> 8u;
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888Unorm << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileRenderTarget64KB << 20u) | (Type2D << 28u),
        0u,
        0u,
        (1u << 20u) | (1u << 21u) | (static_cast<std::uint32_t>(meta & 0xffu) << 24u),
        static_cast<std::uint32_t>(meta >> 8u),
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

struct Scenario {
    std::uint8_t* texels;
    std::uint8_t* keys;
    std::uint8_t* source;
    std::uint32_t* output;
    std::vector<std::uint32_t> copyData;
    std::vector<std::uint32_t> sampleData;
};

Scenario Prepare(std::uint8_t* block, std::uint8_t* outputBlock) {
    Scenario scenario{block, block + KeysOffset, block + SourceKeysOffset, reinterpret_cast<std::uint32_t*>(outputBlock), std::vector<std::uint32_t>(16, 0u), std::vector<std::uint32_t>(24, 0u)};
    const auto source = BufferDescriptor(scenario.source, KeyBytes);
    const auto destination = BufferDescriptor(scenario.keys, KeyBytes);
    std::copy(source.begin(), source.end(), scenario.copyData.begin());
    std::copy(destination.begin(), destination.end(), scenario.copyData.begin() + 4);
    const auto results = BufferDescriptor(scenario.output, Threads * 16u);
    const auto texture = TextureDescriptor(scenario.texels, scenario.keys);
    std::copy(results.begin(), results.end(), scenario.sampleData.begin());
    std::copy(texture.begin(), texture.end(), scenario.sampleData.begin() + 4);
    std::copy(PointSampler.begin(), PointSampler.end(), scenario.sampleData.begin() + 12);
    return scenario;
}

void Reset(const Scenario& scenario) {
    std::memset(scenario.texels, 0x55, SurfaceBytes);
    std::memset(scenario.keys, 0xff, KeyBytes);
    std::memset(scenario.source, 0x00, KeyBytes);
    std::fill(scenario.output, scenario.output + Threads * 4u, 0xdeadbeefu);
    AgcDriver::GuestMemory::BumpCollectEpoch();
}

void RecordKeyCopy(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    const auto copy = Compile(device, CopyKeysCode, scenario.copyData);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    device.Dispatch(copy, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyKeysCode.data()));
    Require(AgcDriver::Graphics::DccKeyWritersPending(AddressOf(scenario.keys), KeyBytes), "a recorded kernel that copies data over the keys is not seen as a pending key writer");
}

void RequireSampledClear(const Scenario& scenario, const char* what) {
    for (std::uint32_t index = 0; index < Threads * 4u; ++index) {
        char message[256];
        std::snprintf(message, sizeof(message), "%s: component %u of thread %u is 0x%08x, expected 0 (keys stored 0000 by the kernel)", what, index % 4u, index / 4u, scenario.output[index]);
        Require(scenario.output[index] == 0u, message);
    }
}

void RunLockedSync(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    Reset(scenario);
    RecordKeyCopy(device, scenario);
    const auto sample = Compile(device, SampleSecondTexelCode, scenario.sampleData);
    const auto waits = AgcDriver::Graphics::KeyWaitCounts();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.Dispatch(sample, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()));
        device.WaitIdle();
    }
    const auto after = AgcDriver::Graphics::KeyWaitCounts();
    Require(after.synced > waits.synced, "a dispatch built under the GPU mutex did not wait for the kernel that copies data over the keys");
    RequireSampledClear(scenario, "a dispatch built under the GPU mutex");
}

void RunPresync(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    Reset(scenario);
    RecordKeyCopy(device, scenario);
    const auto sample = Compile(device, SampleSecondTexelCode, scenario.sampleData);
    const auto waits = AgcDriver::Graphics::KeyWaitCounts();
    const auto presyncs = AgcDriver::VulkanDevice::PresyncCounts();
    auto prepared = device.PrepareDispatch(sample, {}, Threads, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()));
    const auto stageA = AgcDriver::VulkanDevice::PresyncCounts();
    Require(stageA.keyChecks > presyncs.keyChecks, "stage A did not list the DCC keys of the sampled texture for the pre-sync");
    Require(stageA.keyWaits == presyncs.keyWaits + 1 && stageA.presyncs == presyncs.presyncs + 1, "stage A did not wait for the kernel that copies data over the keys before the GPU mutex");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.Dispatch(sample, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()), std::move(prepared));
        device.WaitIdle();
    }
    const auto after = AgcDriver::Graphics::KeyWaitCounts();
    Require(after.synced == waits.synced, "the dispatch waited for the key writer under the GPU mutex after the pre-sync waited for it");
    RequireSampledClear(scenario, "a dispatch whose keys were pre-synced");
}

bool CanFillKeys(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    const std::array<std::uint32_t, 4> pattern{0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const bool filled = device.FillBuffer(AddressOf(scenario.keys), KeyBytes, pattern);
    device.WaitIdle();
    return filled;
}

void RunKnownKeys(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    Reset(scenario);
    const std::array<std::uint32_t, 4> pattern{0u, 0u, 0u, 0u};
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(device.FillBuffer(AddressOf(scenario.keys), KeyBytes, pattern), "the fill of the keys was not recorded");
        Require(AgcDriver::Graphics::DccKeyWritersPending(AddressOf(scenario.keys), KeyBytes), "a recorded fill of the keys is not seen as a pending key writer");
        Require(!AgcDriver::Graphics::DccKeyWriterNeedsSync(AddressOf(scenario.keys), KeyBytes), "the keys of a recorded constant fill need a synchronization");
    }
    const auto sample = Compile(device, SampleSecondTexelCode, scenario.sampleData);
    const auto waits = AgcDriver::Graphics::KeyWaitCounts();
    const auto before = AgcDriver::VulkanDevice::PresyncCounts();
    auto prepared = device.PrepareDispatch(sample, {}, Threads, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()));
    const auto after = AgcDriver::VulkanDevice::PresyncCounts();
    Require(after.keyChecks > before.keyChecks, "stage A did not list the DCC keys of the sampled texture for the pre-sync");
    Require(after.presyncs == before.presyncs, "stage A waited for a fill whose keys the locked lookup derives from the pending stores");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.Dispatch(sample, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()), std::move(prepared));
        device.WaitIdle();
    }
    Require(AgcDriver::Graphics::KeyWaitCounts().synced == waits.synced, "the dispatch synchronized with the GPU although the pending fill gives the keys");
    RequireSampledClear(scenario, "a dispatch behind a pending fill of the keys");
}

void RunMixedWordKeys(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    Reset(scenario);
    const std::array<std::uint32_t, 4> pattern{0x00ff00ffu, 0x00ff00ffu, 0x00ff00ffu, 0x00ff00ffu};
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(device.FillBuffer(AddressOf(scenario.keys), KeyBytes, pattern), "the word fill of the keys was not recorded");
        Require(AgcDriver::Graphics::DccKeyWritersPending(AddressOf(scenario.keys), KeyBytes), "a recorded word fill of the keys is not seen as a pending key writer");
        Require(!AgcDriver::Graphics::DccKeyWriterNeedsSync(AddressOf(scenario.keys), KeyBytes), "the keys of a recorded fill of a word with different bytes need a synchronization");
    }
    const auto sample = Compile(device, SampleSecondTexelCode, scenario.sampleData);
    const auto waits = AgcDriver::Graphics::KeyWaitCounts();
    const auto before = AgcDriver::VulkanDevice::PresyncCounts();
    auto prepared = device.PrepareDispatch(sample, {}, Threads, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()));
    const auto after = AgcDriver::VulkanDevice::PresyncCounts();
    Require(after.keyChecks > before.keyChecks, "stage A did not list the DCC keys of the sampled texture for the pre-sync");
    Require(after.presyncs == before.presyncs, "stage A waited for a word fill whose keys the locked lookup derives from the pending stores");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.Dispatch(sample, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()), std::move(prepared));
        device.WaitIdle();
    }
    Require(AgcDriver::Graphics::KeyWaitCounts().synced == waits.synced, "the dispatch synchronized with the GPU although the pending word fill gives the keys");
    for (std::uint32_t index = 0; index < Threads * 4u; ++index) {
        float value = 0.0f;
        std::memcpy(&value, &scenario.output[index], sizeof(value));
        char message[256];
        std::snprintf(message, sizeof(message), "a dispatch behind a pending fill of mixed keys: component %u of thread %u is %g, expected the stored texel 0x55 (%g)", index % 4u, index / 4u, value, 85.0 / 255.0);
        Require(std::fabs(value - 85.0f / 255.0f) < 1.0f / 512.0f, message);
    }
}

void RunWordFillKeyBytes(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    Reset(scenario);
    constexpr std::uint32_t word = 0x33221100u;
    const std::array<std::uint32_t, 4> pattern{word, word, word, word};
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Require(device.FillBuffer(AddressOf(scenario.keys), KeyBytes, pattern), "the word fill of the keys was not recorded");
    for (std::uint64_t offset = 0; offset < 8u; ++offset) {
        const auto key = AgcDriver::Graphics::CurrentUniformMetadataByte(AddressOf(scenario.keys) + offset, 1);
        const auto expected = static_cast<std::uint8_t>(word >> (8u * (offset % 4u)));
        char message[160];
        std::snprintf(message, sizeof(message), "key byte %llu behind a pending fill of the word 0x%08x is %d, expected 0x%02x", static_cast<unsigned long long>(offset), word, key.has_value() ? static_cast<int>(*key) : -1, expected);
        Require(key == expected, message);
    }
    Require(!AgcDriver::Graphics::CurrentUniformMetadataByte(AddressOf(scenario.keys) + 1, 2).has_value(), "two key bytes of different values behind a pending word fill read as one key");
    device.WaitIdle();
}

void RunOneKeyReadPerLookup(AgcDriver::VulkanDevice& device, const Scenario& scenario, const char* what) {
    Reset(scenario);
    const auto sample = Compile(device, SampleSecondTexelCode, scenario.sampleData);
    auto prepared = device.PrepareDispatch(sample, {}, Threads, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()));
    const auto before = AgcDriver::Graphics::SampledLookupKeyScans();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.Dispatch(sample, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()), std::move(prepared));
        device.WaitIdle();
    }
    const auto scans = AgcDriver::Graphics::SampledLookupKeyScans() - before;
    char message[192];
    std::snprintf(message, sizeof(message), "%s scanned the keys of its sampled texture %llu times with nothing written between the scans, expected at most once", what, static_cast<unsigned long long>(scans));
    Require(scans <= 1u, message);
}

void RunNoWriter(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    const auto sample = Compile(device, SampleSecondTexelCode, scenario.sampleData);
    const auto before = AgcDriver::VulkanDevice::PresyncCounts();
    Require(!AgcDriver::Graphics::DccKeyWritersPending(AddressOf(scenario.keys), KeyBytes), "the idle device still reports a pending key writer");
    auto prepared = device.PrepareDispatch(sample, {}, Threads, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()));
    const auto after = AgcDriver::VulkanDevice::PresyncCounts();
    Require(after.keyChecks > before.keyChecks, "stage A did not list the DCC keys of the sampled texture for the pre-sync");
    Require(after.keyWaits == before.keyWaits && after.presyncs == before.presyncs, "stage A waited although no recorded work writes the keys");
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    device.Dispatch(sample, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleSecondTexelCode.data()), std::move(prepared));
    device.WaitIdle();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->CanWaitUnlocked()) {
            std::puts("skipped, the device has no timeline semaphores: the pre-sync is not tested");
            return VulkanTestSkipped;
        }
        GuestBlock locked;
        GuestBlock lockedOutput;
        GuestBlock presynced;
        GuestBlock presyncedOutput;
        GuestBlock known;
        GuestBlock knownOutput;
        const auto control = Prepare(locked.Data(), lockedOutput.Data());
        const auto changed = Prepare(presynced.Data(), presyncedOutput.Data());
        const auto fills = Prepare(known.Data(), knownOutput.Data());
        if (!CanFillKeys(*device, control)) {
            std::puts("skipped, host imports are unavailable: the key writers of the test are copied writers");
            return VulkanTestSkipped;
        }
        RunLockedSync(*device, control);
        RunPresync(*device, changed);
        RunNoWriter(*device, changed);
        GuestBlock single;
        GuestBlock singleOutput;
        const auto reads = Prepare(single.Data(), singleOutput.Data());
        RunOneKeyReadPerLookup(*device, reads, "a dispatch's first lookup");
        RunOneKeyReadPerLookup(*device, reads, "a dispatch's repeated lookup");
        RunKnownKeys(*device, fills);
        RunMixedWordKeys(*device, fills);
        RunWordFillKeyBytes(*device, fills);
        std::puts("DCC key pre-sync tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
