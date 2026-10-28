#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <span>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::uint32_t Width = 128;
constexpr std::size_t SurfaceBytes = 0x20000;
constexpr std::size_t PayloadOffset = 0x15400;
alignas(256) constexpr std::array<std::uint32_t, 11> WriteCode{
    0xd7460004u, 0x0401060eu, 0xd7460005u, 0x0405060fu, 0x7e100d04u, 0x7e120d05u,
    0x7e140c0du, 0x7e1602f2u, 0xf0200f08u, 0x00000804u, 0xbf810000u,
};

class GuestSurface {
public:
    explicit GuestSurface(bool registerMemory = true) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, SurfaceBytes));
        GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "surface byte copy: cannot allocate guest memory");
        const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
        const auto aligned = (begin + SurfaceBytes - 1) & ~std::uintptr_t{SurfaceBytes - 1};
        if (aligned != begin) munmap(mapped, aligned - begin);
        const auto tail = begin + SurfaceBytes * 2 - aligned - SurfaceBytes;
        if (tail != 0) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), tail);
        block = reinterpret_cast<std::uint8_t*>(aligned);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
#endif
        if (registerMemory) Register();
    }

    ~GuestSurface() {
        if (registered) {
            GuestAllocations::Mutation mutation;
            mutation.Remove(block);
        }
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, SurfaceBytes);
#else
        munmap(block, SurfaceBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, SurfaceBytes);
#endif
    }

    GuestSurface(const GuestSurface&) = delete;
    GuestSurface& operator=(const GuestSurface&) = delete;
    std::uint8_t* Data() { return block; }
    void Register() {
        Require(!registered, "surface byte copy: memory is already registered");
        GuestAllocations::Mutation().Add(block, SurfaceBytes, true, true, true);
        registered = true;
    }

private:
    std::uint8_t* block = nullptr;
    bool registered = false;
};

void CheckPreflight(AgcDriver::VulkanDevice& device, GuestSurface& source, GuestSurface& destination, std::uint32_t height, bool& unavailable) {
    std::vector<std::byte> initial(SurfaceBytes), actual(SurfaceBytes);
    for (std::size_t at = 0; at < SurfaceBytes; ++at) initial[at] = static_cast<std::byte>(0x31u + at % 67u);
    std::memcpy(source.Data(), initial.data(), initial.size());
    std::memset(destination.Data(), 0xa5, SurfaceBytes);
    const auto from = reinterpret_cast<std::uintptr_t>(source.Data());
    const auto to = reinterpret_cast<std::uintptr_t>(destination.Data());
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const bool expectedAlias = height == 128 && std::getenv("APS5_NO_COPY_ALIAS") == nullptr;
    AgcDriver::VulkanDevice::CopyOutcome outcome{};
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        if (AgcDriver::Graphics::HostImportFor(context, to, SurfaceBytes) == nullptr) {
            unavailable = true;
            return;
        }
        Require(AgcDriver::Graphics::HostImportFor(context, from, SurfaceBytes) == nullptr, "surface byte copy: preflight source unexpectedly has a host import");
        AgcDriver::Graphics::GuestTextureResource resource{};
        resource.baseAddress = from;
        resource.width = Width;
        resource.height = height;
        resource.mipCount = 1;
        resource.tileMode = AgcDriver::Graphics::TextureTileMode::kStandard64KB;
        resource.dimension = AgcDriver::Graphics::TextureDimension::k2D;
        resource.format = 71;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto image = AgcDriver::Graphics::CachedStorageSurface(context, resource);
        Require(image->GuestBytes() == SurfaceBytes && image->ImageCoversGuestBytes() == (height == 128), "surface byte copy: preflight image has unexpected byte coverage");
        Require(device.GpuCopyPossible(to, from, SurfaceBytes) == expectedAlias, "surface byte copy: preflight accepted incomplete image bytes or refused a dense alias");
        if (!expectedAlias) {
            source.Register();
            Require(AgcDriver::Graphics::HostImportFor(context, from, SurfaceBytes) != nullptr, "surface byte copy: registered preflight source cannot be imported");
        }
        Require(device.GpuCopyPossible(to, from, SurfaceBytes), "surface byte copy: preflight refused the available GPU copy");
        outcome = device.CopyBuffer(to, from, SurfaceBytes, 0, std::numeric_limits<std::size_t>::max(), 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
        Require(outcome.path == (expectedAlias ? 3 : 1), "surface byte copy: preflight and execution selected different GPU paths");
    }
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        device.WaitIdle();
    }
    AgcDriver::GuestMemory::Read(to, actual);
    std::size_t differences = 0;
    for (std::size_t at = 0; at < SurfaceBytes; ++at) differences += initial[at] != actual[at];
    std::printf("preflight height=%u unimported_allowed=%d copy_path=%d incorrect_bytes=%zu\n", height, expectedAlias, static_cast<int>(outcome.path), differences);
    Require(differences == 0, "surface byte copy: preflight GPU path did not preserve all raw bytes");
}

void Drain() {
    AgcDriverSuspendPoint_nid_postfix();
    AgcDriverWaitIdle_nid_postfix();
}

void Produce(GuestSurface& surface, std::uint32_t height) {
    const auto address = reinterpret_cast<std::uintptr_t>(surface.Data());
    const std::array<std::uint32_t, 14> data{
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (71u << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (0x09u << 20u) | (9u << 28u),
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 7u,
    };
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{8, 8, 1};
    registers(0x207, shape);
    const auto code = reinterpret_cast<std::uintptr_t>(WriteCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (7u << 7u) | (1u << 11u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, Width / 8u, height / 8u, 1u, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    Drain();
    Require(StorageTexture::FindPending(address, SurfaceBytes) != nullptr, "surface byte copy: producer left no pending image");
}

void Copy(void* destination, const void* source, std::uint32_t bytes) {
    const auto from = reinterpret_cast<std::uintptr_t>(source);
    const auto to = reinterpret_cast<std::uintptr_t>(destination);
    std::array<std::uint32_t, 7> words{
        0xc0055000u, 0x60000000u, static_cast<std::uint32_t>(from), static_cast<std::uint32_t>(from >> 32u),
        static_cast<std::uint32_t>(to), static_cast<std::uint32_t>(to >> 32u), bytes,
    };
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    Drain();
}

bool Imported(GuestSurface& surface) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "surface byte copy: no active recorder");
    AgcDriver::Graphics::Context probe{};
    probe.device = recorder->Device();
    probe.hostImportAlignment = 1;
    return AgcDriver::Graphics::HostImportCovers(probe, reinterpret_cast<std::uintptr_t>(surface.Data()), SurfaceBytes);
}

bool Check(GuestSurface& source, GuestSurface& destination, GuestSurface& reader, std::uint32_t height, bool cachedDestination, bool& unavailable) {
    std::vector<std::byte> initial(SurfaceBytes), expected(SurfaceBytes), actual(SurfaceBytes);
    for (std::size_t at = 0; at < SurfaceBytes; ++at) initial[at] = static_cast<std::byte>(0x31u + at % 67u);
    std::memcpy(source.Data(), initial.data(), initial.size());
    std::memset(destination.Data(), 0xa5, SurfaceBytes);
    if (cachedDestination) Produce(destination, height);
    Produce(source, height);
    const auto before = AgcDriver::VulkanDevice::CopyAliasCounts();
    Copy(destination.Data(), source.Data(), SurfaceBytes);
    const auto after = AgcDriver::VulkanDevice::CopyAliasCounts();
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(source.Data()), expected);
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(destination.Data()), actual);
    Require(expected != initial, "surface byte copy: GPU producer did not change its source");
    std::size_t differences = 0;
    for (std::size_t at = 0; at < SurfaceBytes; ++at) differences += expected[at] != actual[at];
    std::memset(reader.Data(), 0xcd, SurfaceBytes);
    Copy(reader.Data(), destination.Data() + PayloadOffset, 4);
    std::array<std::byte, 4> observed{};
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(reader.Data()), observed);
    std::uint32_t expectedWord = 0, actualWord = 0;
    std::memcpy(&expectedWord, expected.data() + PayloadOffset, sizeof(expectedWord));
    std::memcpy(&actualWord, observed.data(), sizeof(actualWord));
    const auto aliased = after.aliased - before.aliased;
    const auto created = after.created - before.created;
    std::printf("height=%u cached_destination=%d image_copies=%llu created=%llu incorrect_bytes=%zu expected_payload=%08x actual_payload=%08x\n", height, cachedDestination, static_cast<unsigned long long>(aliased), static_cast<unsigned long long>(created), differences, expectedWord, actualWord);
    const bool fullyCovered = height == 128;
    const bool aliasesDisabled = std::getenv("APS5_NO_COPY_ALIAS") != nullptr;
    const bool expectedAlias = fullyCovered && !aliasesDisabled;
    const bool noImport = expectedAlias && aliased == 0 && !Imported(destination);
    unavailable = unavailable || noImport;
    const bool pathCorrect = aliased == (expectedAlias ? 1u : 0u) && created == (expectedAlias && !cachedDestination ? 1u : 0u);
    return differences == 0 && actualWord == expectedWord && (noImport || pathCorrect);
}

}

int main() {
    try {
        bool unavailable = false;
        {
            GuestSurface preflightPaddedSource(false), preflightPaddedDestination, preflightDenseSource(false), preflightDenseDestination;
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            CheckPreflight(*device, preflightPaddedSource, preflightPaddedDestination, 120, unavailable);
            CheckPreflight(*device, preflightDenseSource, preflightDenseDestination, 128, unavailable);
        }
        GuestSurface paddedSource, paddedDestination, cachedSource, cachedDestination, denseSource, denseDestination, reader;
        bool correct = Check(paddedSource, paddedDestination, reader, 120, false, unavailable);
        correct = Check(cachedSource, cachedDestination, reader, 120, true, unavailable) && correct;
        correct = Check(denseSource, denseDestination, reader, 128, false, unavailable) && correct;
        Require(correct, "surface byte copy: image alias did not preserve the raw byte copy");
        AgcDriverShutdown_nid_postfix();
        if (unavailable) {
            std::puts("skipped, the device does not import the dense image copy destination");
            return VulkanTestSkipped;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
