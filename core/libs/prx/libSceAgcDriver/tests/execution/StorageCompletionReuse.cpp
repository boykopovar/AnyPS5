#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceAgcDriver/Eq/include/Query.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::size_t PageBlock = 0x10000;
constexpr std::uint32_t Texel = 0x00ffffffu;
constexpr std::uint8_t Initial = 0xa5;
constexpr std::uint8_t Guard = 0x3c;

alignas(256) constexpr std::array<std::uint32_t, 8> StoreCode{
    0x7e02020c, 0x7e0402f2, 0x7e0602f2, 0x7e0802f2, 0x7e0a0280, 0xf0201f08, 0x00010200, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> LoadRowCode{
    0x7e020280, 0xf0001108, 0x00010300, 0x34080082, 0xbf8c3f70, 0xe0701000, 0x80000304, 0xbf810000,
};

std::uint64_t Address(const void* pointer) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

struct Shape {
    std::uint32_t width;
    std::uint32_t height;
    std::size_t bytes;
};

std::array<std::uint32_t, 8> Descriptor(std::uint64_t address, Shape shape) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (((shape.width - 1u) & 3u) << 30u),
        ((shape.width - 1u) >> 2u) | ((shape.height - 1u) << 14u),
        0xfacu | (0x1bu << 20u) | (9u << 28u),
        0u, 0u, 0u, 0u,
    };
}

class GuestBlock {
public:
    GuestBlock(std::size_t imageBytes, bool watched) : bytes(imageBytes + 2 * PageBlock), watched(watched) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, PageBlock));
            if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, PageBlock);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        void* mapped = mmap(nullptr, bytes + PageBlock, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + PageBlock - 1) & ~std::uintptr_t{PageBlock - 1};
            if (aligned != begin) munmap(mapped, aligned - begin);
            const auto tail = begin + bytes + PageBlock - aligned - bytes;
            if (tail != 0) munmap(reinterpret_cast<void*>(aligned + bytes), tail);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            if (watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        }
#endif
        Require(data != nullptr, "storage completion reuse: cannot allocate guest memory");
        std::memset(data, Guard, bytes);
        GuestAllocations::Mutation().Add(data, bytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        munmap(data, bytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint8_t* Data() const { return data + PageBlock; }

    std::size_t ChangedGuards() const {
        const volatile std::uint8_t* cpu = data;
        std::size_t changed = 0;
        for (std::size_t offset = 0; offset < PageBlock; ++offset) {
            changed += cpu[offset] != Guard;
            changed += cpu[bytes - PageBlock + offset] != Guard;
        }
        return changed;
    }

private:
    std::size_t bytes;
    bool watched;
    std::uint8_t* data = nullptr;
};

enum class Route { Label32, Label64, Timestamp, CacheOnly, Interrupt, Conditional, NoInterrupt, Compute, Consumer };

class Events {
public:
    explicit Events(bool enabled) {
        if (!enabled) return;
        Require(sceKernelCreateEqueue(&queue, "AGC storage completion") == 0, "storage completion reuse: cannot create event queue");
        owner = EqueuePin_nid_postfix(queue);
        Require(owner != nullptr, "storage completion reuse: cannot pin event queue");
        Require(sceAgcDriverAddEqEvent(queue, 0, &tag) == 0, "storage completion reuse: cannot register end-of-pipe event");
    }

    ~Events() {
        if (queue == 0) return;
        sceAgcDriverDeleteEqEvent(queue, 0);
        owner.reset();
        sceKernelDeleteEqueue(queue);
    }

    void Drain() {
        if (owner == nullptr) return;
        std::array<KernelEvent, 1> event{};
        while (owner->GetTriggeredEvents(event.data(), 1) != 0) {}
    }

    bool Arrived(std::uint32_t context) {
        std::array<KernelEvent, 1> event{};
        if (owner->GetTriggeredEvents(event.data(), 1) == 0) return false;
        Require(event[0].udata == &tag && sceAgcDriverGetEqContextId(event.data()) == context,
                "storage completion reuse: an unexpected end-of-pipe event arrived");
        return true;
    }

private:
    KernelEqueue queue = 0;
    KernelEqueueRef owner;
    int tag = 0;
};

void StoreThenLabel(const GuestBlock& image, Shape shape, std::uint32_t* label, std::uint32_t value, std::uint32_t releaseFlags, Route route, const GuestBlock* output) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> threads{shape.width, 1, 1};
    registers(0x207, threads);
    const auto code = Address(StoreCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    registers(0x20c, program);
    std::array<std::uint32_t, 12> userData{};
    const auto texture = Descriptor(Address(image.Data()), shape);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<std::uint32_t, 1> layout{(static_cast<std::uint32_t>(userData.size()) << 1u) | 0x80u};
    registers(0x213, layout);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, shape.height, 1, 1, 0x41u});
    const auto destination = Address(label);
    const auto release = [&](std::uint32_t flags, std::uint32_t dataSelect, std::uint32_t interruptSelect, std::uint64_t address, std::uint32_t low, std::uint32_t high) {
        words.insert(words.end(), {0xc0064900u, flags, (dataSelect << 29u) | (interruptSelect << 24u),
                static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), low, high, value});
    };
    if (route == Route::Consumer) {
        Require(output != nullptr, "storage completion reuse: consumer output is absent");
        release(releaseFlags, 0, 0, 0, 0, 0);
        const auto readCode = Address(LoadRowCode.data());
        const std::array<std::uint32_t, 2> readProgram{static_cast<std::uint32_t>(readCode >> 8u), static_cast<std::uint32_t>(readCode >> 40u)};
        registers(0x20c, readProgram);
        const auto outputAddress = Address(output->Data());
        std::array<std::uint32_t, 16> readData{};
        readData[0] = static_cast<std::uint32_t>(outputAddress);
        readData[1] = static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu);
        readData[2] = shape.width * sizeof(Texel);
        readData[3] = 0x31016facu;
        auto linear = texture;
        linear[1] = (linear[1] & ~(0x1ffu << 20u)) | (20u << 20u);
        linear[3] = 0xfacu | (9u << 28u);
        std::copy(linear.begin(), linear.end(), readData.begin() + 4);
        const std::array<std::uint32_t, 1> readLayout{static_cast<std::uint32_t>(readData.size()) << 1u};
        registers(0x213, readLayout);
        registers(0x240, readData);
        words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
        release(0x514u, 1, 0, destination, value, 0);
    } else if (route == Route::CacheOnly) {
        release(releaseFlags, 0, 0, 0, 0, 0);
        release(0x514u, 1, 0, destination, value, 0);
    } else if (route == Route::Interrupt) {
        release(releaseFlags, 0, 2, 0, 0, 0);
    } else {
        const auto dataSelect = route == Route::Label64 ? 2u : route == Route::Timestamp ? 3u : 1u;
        const auto interruptSelect = route == Route::Conditional ? 3u : route == Route::NoInterrupt ? 0u : 2u;
        release(releaseFlags, dataSelect, interruptSelect, destination, value, route == Route::Label64 ? 0x12345678u : 0u);
    }
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, route == Route::Compute ? 0x20u : 0u);
}

std::uint64_t ClockStamp() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 10);
}

void AwaitCompletion(std::uint32_t* label, std::uint32_t value, Route route, Events& events, std::uint64_t started) {
    const auto complete = [&] {
        if (route == Route::Interrupt) return events.Arrived(value);
        if (route == Route::Label64 || route == Route::Timestamp) {
            const auto observed = std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(label)).load(std::memory_order_acquire);
            return route == Route::Timestamp ? observed >= started && observed <= ClockStamp() : observed == ((0x12345678ull << 32u) | value);
        }
        return std::atomic_ref<std::uint32_t>(*label).load(std::memory_order_acquire) == value;
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!complete()) {
        Require(std::chrono::steady_clock::now() < deadline, "storage completion reuse: completion did not arrive");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

bool LabelMatches(std::uint32_t* label, std::uint32_t value, Route route, std::uint64_t started, std::uint64_t completed) {
    const auto observed = std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(label)).load(std::memory_order_acquire);
    if (route == Route::Timestamp) return observed >= started && observed <= completed;
    if (route == Route::Interrupt) return observed == UINT64_MAX;
    const auto high = route == Route::Label64 ? 0x12345678ull : 0xffffffffull;
    return observed == ((high << 32u) | value);
}

std::uint64_t Link(std::uint32_t pass, std::size_t offset) {
    return 0x0000000280000010ull + (static_cast<std::uint64_t>(pass) << 24u) + offset;
}

void WriteLinks(std::uint8_t* image, std::size_t bytes, std::uint32_t pass) {
    for (std::size_t block = 0; block < bytes; block += PageBlock) {
        for (const auto within : {std::size_t{0}, std::size_t{8}, PageBlock - sizeof(std::uint64_t)}) {
            const auto offset = block + within;
            const auto link = Link(pass, offset);
            std::memcpy(image + offset, &link, sizeof(link));
        }
    }
}

struct Comparison {
    std::size_t differences = 0;
    std::size_t first = 0;
};

Comparison Compare(const std::uint8_t* actual, std::span<const std::uint8_t> expected) {
    const volatile std::uint8_t* cpu = actual;
    Comparison result;
    for (std::size_t offset = 0; offset < expected.size(); ++offset) {
        if (cpu[offset] == expected[offset]) continue;
        if (result.differences == 0) result.first = offset;
        ++result.differences;
    }
    return result;
}

void Publish(const GuestBlock& image, std::size_t bytes) {
    std::array<std::byte, 16> read{};
    AgcDriver::GuestMemory::Read(Address(image.Data()) + bytes / 2, read);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    StorageTexture::FlushAllPending("storage completion reuse fixture");
}

int Run(Shape shape, bool watched, std::uint32_t releaseFlags, Route route, const std::string& routeName) {
    GuestBlock image(shape.bytes, watched);
    GuestBlock labels(PageBlock, false);
    std::unique_ptr<GuestBlock> output;
    std::vector<std::uint8_t> outputBytes;
    if (route == Route::Consumer) {
        output = std::make_unique<GuestBlock>(PageBlock, false);
        outputBytes.resize(PageBlock, Initial);
        for (std::size_t offset = 0; offset < shape.width * sizeof(Texel); offset += sizeof(Texel)) {
            std::memcpy(outputBytes.data() + offset, &Texel, sizeof(Texel));
        }
    }
    Events events(route == Route::Interrupt);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(Descriptor(Address(image.Data()), shape));
    Require(resource.width == shape.width && resource.height == shape.height &&
            AgcDriver::Graphics::DescribeSurface(resource).guestBytes == shape.bytes,
            "storage completion reuse: descriptor does not cover the requested surface");
    const bool imageWatched = AgcDriver::GuestMemory::Watched(Address(image.Data()), shape.bytes);
    if (watched && !imageWatched) {
        std::printf("[storage completion reuse] SKIP: guest write-watch tracking is unavailable for the requested watched allocation\n");
        return VulkanTestSkipped;
    }
    Require(imageWatched == watched, "storage completion reuse: compared allocation is unexpectedly write-watched");
    std::vector<std::uint8_t> gpuBytes(shape.bytes);
    for (std::size_t offset = 0; offset < shape.bytes; offset += sizeof(Texel)) std::memcpy(gpuBytes.data() + offset, &Texel, sizeof(Texel));
    auto* label = reinterpret_cast<std::uint32_t*>(labels.Data());
    std::size_t failures = 0;
    struct Shutdown {
        ~Shutdown() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
    } shutdown;
    for (std::uint32_t pass = 0; pass < 4; ++pass) {
        const bool same = (pass & 1u) != 0;
        std::memset(image.Data(), Initial, shape.bytes);
        WriteLinks(image.Data(), shape.bytes, pass);
        if (output) std::memset(output->Data(), Initial, PageBlock);
        std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(label)).store(UINT64_MAX, std::memory_order_release);
        events.Drain();
        const auto started = ClockStamp();
        StoreThenLabel(image, shape, label, pass + 1, releaseFlags, route, output.get());
        AwaitCompletion(label, pass + 1, route, events, started);
        const auto completed = ClockStamp();
        const auto visible = Compare(image.Data(), gpuBytes);
        const auto consumed = output ? Compare(output->Data(), outputBytes) : Comparison{};
        const auto cpuPass = same ? pass : pass + 100;
        WriteLinks(image.Data(), shape.bytes, cpuPass);
        auto expected = gpuBytes;
        WriteLinks(expected.data(), shape.bytes, cpuPass);
        Publish(image, shape.bytes);
        const auto kept = Compare(image.Data(), expected);
        const auto guards = image.ChangedGuards() + labels.ChangedGuards() + (output ? output->ChangedGuards() : 0);
        const bool labelMatches = LabelMatches(label, pass + 1, route, started, completed);
        std::printf("[storage completion reuse] bytes=%zu memory=%s release=0x%x route=%s pass=%u links=%s before=%zu first=0x%zx after=%zu first=0x%zx consumed=%zu first=0x%zx guards=%zu label=%d\n",
                    shape.bytes, watched ? "watched" : "compared", releaseFlags, routeName.c_str(), pass, same ? "same" : "new",
                    visible.differences, visible.first, kept.differences, kept.first, consumed.differences, consumed.first, guards, labelMatches);
        failures += visible.differences != 0 || kept.differences != 0 || consumed.differences != 0 || guards != 0 || !labelMatches;
    }
    AgcDriverWaitIdle_nid_postfix();
    if (releaseFlags == 0x514u) {
        std::printf("[storage completion reuse] diagnostic only: legacy release had %zu differing passes; its cache visibility contract is not asserted\n", failures);
        return 0;
    }
    Require(failures == 0, "storage completion reuse: completion visibility or later CPU ownership was lost");
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || (argc >= 3 && argc <= 5), "storage completion reuse: expected size, memory mode and optional release mode and route");
        const std::string size = argc == 1 ? "medium" : argv[1];
        const Shape shape = size == "small" ? Shape{128, 128, PageBlock} :
            size == "medium" ? Shape{256, 256, 4 * PageBlock} :
            size == "large" ? Shape{256, 2048, 32 * PageBlock} : Shape{};
        Require(shape.bytes != 0, "storage completion reuse: unknown size");
        const std::string memory = argc == 1 ? "compared" : argv[2];
        const std::string release = argc < 4 ? "explicit" : argv[3];
        Require(release == "explicit" || release == "writeback" || release == "legacy", "storage completion reuse: unknown release mode");
        std::uint32_t releaseFlags = release == "explicit" ? 0x00300514u : release == "writeback" ? 0x00200514u : 0x514u;
        Require(memory == "watched" || memory == "compared", "storage completion reuse: unknown memory mode");
        const std::string routeName = argc < 5 ? "label32" : argv[4];
        Route route = Route::Label32;
        if (routeName == "label64") route = Route::Label64;
        else if (routeName == "timestamp") route = Route::Timestamp;
        else if (routeName == "cache-only") route = Route::CacheOnly;
        else if (routeName == "interrupt") route = Route::Interrupt;
        else if (routeName == "conditional") route = Route::Conditional;
        else if (routeName == "no-interrupt") route = Route::NoInterrupt;
        else if (routeName == "compute") { route = Route::Compute; releaseFlags = 0x0020062fu; }
        else if (routeName == "consumer") route = Route::Consumer;
        else Require(routeName == "label32", "storage completion reuse: unknown route");
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        return Run(shape, memory == "watched", releaseFlags, route, routeName);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
