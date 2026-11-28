#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "../WatchedBlock.hpp"
#include "BatchHold.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 0x40000;
constexpr std::uint64_t ColorOffset = 0x30000;
constexpr std::size_t ColorBytes = 1024;
constexpr std::uint64_t OpeningFillOffset = 0x10000;
constexpr std::uint32_t OpeningFillBytes = 0x10010;

alignas(256) constexpr std::array<std::uint32_t, 16> TriangleVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> ConstantPixelCode{0xf4000080u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(256) constexpr std::array<std::uint32_t, 16> PairPixelCode{0xf4040080u, 0xfa000000u, 0xbf8cc07fu, 0x7e000203u, 0x06000002u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};
alignas(8) Header PairPixelHeader{};

void Register(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

class GuestBlock {
public:
    GuestBlock() : block(static_cast<std::uint8_t*>(AllocateWatched(BlockBytes, 65536))) {
        if (block == nullptr) return;
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        if (block == nullptr) return;
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), BlockBytes, nullptr, "test release");
        GuestAllocations::Mutation().Remove(block);
        ReleaseWatched(block, BlockBytes);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    bool Valid() const { return block != nullptr; }
    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

bool LabelWordWatched(const GuestBlock& block, std::uint64_t offset) {
    return AgcDriver::GuestMemory::Watched(block.Address() + offset, sizeof(std::uint32_t));
}

void CollectSetupStores(const GuestBlock& block) {
    static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(block.Address(), BlockBytes));
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void AppendState(std::vector<std::uint32_t>& commands, std::uint64_t color, const std::uint32_t* pixelCode = ConstantPixelCode.data()) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(TriangleVertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(pixelCode);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 0}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 2u << 1u},
    };
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
}

void AppendLabel(std::vector<std::uint32_t>& commands, std::uint64_t address, std::uint32_t value) {
    Append(commands, 0x37, {0x00100200u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value});
}

void AppendFill(std::vector<std::uint32_t>& commands, std::uint64_t address, std::uint32_t pattern, std::uint32_t bytes) {
    Append(commands, 0x50, {0x40000000u, pattern, 0, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), bytes});
}

void AppendOpeningFill(std::vector<std::uint32_t>& commands, std::uint64_t base) {
    static std::uint32_t openingFills = 0;
    AppendFill(commands, base + OpeningFillOffset, ++openingFills, OpeningFillBytes);
}

void AppendConstantPointer(std::vector<std::uint32_t>& commands, std::uint64_t constant) {
    Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(constant), static_cast<std::uint32_t>(constant >> 32u)});
}

void AppendDraw(std::vector<std::uint32_t>& commands) {
    Append(commands, 0x2d, {3, 2});
}

void AppendWait(std::vector<std::uint32_t>& commands, std::uint64_t address, std::uint32_t reference) {
    Append(commands, 0x3c, {0x13u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), reference, 0xffffffffu, 0x10u});
}

struct Outcome {
    int brightest = 0;
    std::uint64_t served = 0;
    std::uint64_t mismatches = 0;
    std::uint64_t syncs = 0;
    std::uint64_t stores = 0;
};

struct Scenario {
    std::uint64_t constantOffset;
    float cpuValue;
    std::uint64_t labelOffset;
    float labelValue;
    bool fillAfterLabel;
    float fillValue;
};

Outcome Run(const GuestBlock& block, const Scenario& scenario, bool knownLabels) {
    AgcDriver::Graphics::RuntimeSwitches::Assign(AgcDriver::Graphics::RuntimeSwitch::NoCaptureKnownLabels, !knownLabels);
    const auto cpu = std::bit_cast<std::uint32_t>(scenario.cpuValue);
    std::memcpy(block.Data() + scenario.constantOffset, &cpu, sizeof(cpu));
    std::memset(block.Data() + ColorOffset, 0, ColorBytes);
    CollectSetupStores(block);
    std::vector<std::uint32_t> commands;
    AppendOpeningFill(commands, block.Address());
    AppendState(commands, block.Address() + ColorOffset);
    AppendConstantPointer(commands, block.Address() + scenario.constantOffset);
    AppendLabel(commands, block.Address() + scenario.labelOffset, std::bit_cast<std::uint32_t>(scenario.labelValue));
    if (scenario.fillAfterLabel) AppendFill(commands, block.Address() + (scenario.labelOffset & ~std::uint64_t{0xff}), std::bit_cast<std::uint32_t>(scenario.fillValue), 256);
    AppendDraw(commands);
    const auto storesBefore = AgcDriver::Graphics::Recorder::StoreCounts().stores;
    const auto knownBefore = AgcDriver::ShaderMemory::KnownValues();
    const auto syncsBefore = AgcDriver::Graphics::Recorder::HookSyncCount();
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "capture known label: the submission failed");
    AgcDriverWaitIdle_nid_postfix();
    Outcome outcome;
    outcome.syncs = AgcDriver::Graphics::Recorder::HookSyncCount() - syncsBefore;
    const auto known = AgcDriver::ShaderMemory::KnownValues();
    outcome.served = known.served - knownBefore.served;
    outcome.mismatches = known.mismatches - knownBefore.mismatches;
    outcome.stores = AgcDriver::Graphics::Recorder::StoreCounts().stores - storesBefore;
    AgcDriver::GuestMemory::FlushGpuWrites(block.Address() + ColorOffset, ColorBytes);
    outcome.brightest = *std::max_element(block.Data() + ColorOffset, block.Data() + ColorOffset + ColorBytes);
    return outcome;
}

bool Near(int brightest, int expected) {
    return brightest >= expected - 1 && brightest <= expected + 1;
}

int Check(const GuestBlock& block, const char* name, const Scenario& off, const Scenario& on, int expected, bool servedExpected, bool fewerSyncs) {
    const auto before = Run(block, off, false);
    const auto after = Run(block, on, true);
    const auto verify = std::getenv("APS5_VERIFY_KNOWN_VALUES") != nullptr;
    Require(Near(before.brightest, expected), std::string("capture known label, ") + name + ": the draw wrote " + std::to_string(before.brightest) + " with the switch off, expected " + std::to_string(expected));
    Require(Near(after.brightest, expected), std::string("capture known label, ") + name + ": the draw wrote " + std::to_string(after.brightest) + " with known labels, expected " + std::to_string(expected));
    if (verify) Require(after.mismatches == 0, std::string("capture known label, ") + name + ": a word served from a label differs from what the GPU stored");
    if (before.stores == 0 || after.stores == 0) {
        std::cout << name << ": the label was not recorded on the GPU, so the capture had nothing to wait for\n";
        return 0;
    }
    const bool provable = LabelWordWatched(block, on.labelOffset);
    if (servedExpected && !provable) std::cout << name << ": the label's memory is compared after its host import instead of write-watched, so no CPU store over the word can be ruled out and the word is not served" << '\n';
    if (servedExpected && provable) Require(after.served > before.served, std::string("capture known label, ") + name + ": the label word was not served from the label (" + std::to_string(after.served) + " served, " + std::to_string(before.served) + " with the switch off)");
    if (fewerSyncs && servedExpected) {
        Require(after.syncs <= before.syncs, std::string("capture known label, ") + name + ": known labels added hook syncs (" + std::to_string(after.syncs) + ", " + std::to_string(before.syncs) + " with the switch off)");
        if (after.syncs == before.syncs && provable) std::cout << name << ": the label word was served without waiting, and another access in the same run synced as it did with the switch off" << '\n';
    } else if (fewerSyncs) Require(after.syncs < before.syncs, std::string("capture known label, ") + name + ": the capture still waited for the GPU (" + std::to_string(after.syncs) + " hook syncs, " + std::to_string(before.syncs) + " with the switch off)");
    else Require(after.syncs >= before.syncs || after.served == before.served, std::string("capture known label, ") + name + ": a word with a later non-label write was served without waiting");
    std::cout << name << ": " << after.syncs << " hook syncs and " << after.served << " served words with known labels, " << before.syncs << " and " << before.served << " with the switch off\n";
    return 0;
}

int CheckSubmittedBeforeWait(const GuestBlock& block) {
    AgcDriver::Graphics::RuntimeSwitches::Assign(AgcDriver::Graphics::RuntimeSwitch::NoCaptureKnownLabels, false);
    constexpr std::uint64_t constantOffset = 0x7000;
    constexpr std::uint64_t releaseOffset = 0x8000;
    const auto label = std::bit_cast<std::uint32_t>(1.0f);
    const auto cpu = std::bit_cast<std::uint32_t>(0.25f);
    std::memcpy(block.Data() + constantOffset, &cpu, sizeof(cpu));
    std::memset(block.Data() + releaseOffset, 0, sizeof(std::uint32_t));
    std::memset(block.Data() + ColorOffset, 0, ColorBytes);
    CollectSetupStores(block);
    std::vector<std::uint32_t> commands;
    AppendOpeningFill(commands, block.Address());
    AppendState(commands, block.Address() + ColorOffset);
    AppendConstantPointer(commands, block.Address() + constantOffset);
    AppendLabel(commands, block.Address() + constantOffset, label);
    AppendDraw(commands);
    AppendWait(commands, block.Address() + releaseOffset, 1);
    const auto storesBefore = AgcDriver::Graphics::Recorder::StoreCounts().stores;
    std::atomic_ref<std::uint32_t> landed(*reinterpret_cast<std::uint32_t*>(block.Data() + constantOffset));
    std::atomic_ref<std::uint32_t> release(*reinterpret_cast<std::uint32_t*>(block.Data() + releaseOffset));
    bool recorded = false;
    bool reached = false;
    std::thread holder([&] {
        const auto recordDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!(recorded = AgcDriver::Graphics::Recorder::StoreCounts().stores != storesBefore) && landed.load(std::memory_order_acquire) != label && std::chrono::steady_clock::now() < recordDeadline) std::this_thread::yield();
        auto& gpu = AgcDriver::GuestMemory::GpuMutex();
        const auto holdDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (recorded && !(reached = landed.load(std::memory_order_acquire) == label) && std::chrono::steady_clock::now() < holdDeadline) {
            {
                std::lock_guard hold(gpu);
                while (gpu.Waiters() == 0 && landed.load(std::memory_order_acquire) != label && std::chrono::steady_clock::now() < holdDeadline) std::this_thread::yield();
            }
            std::this_thread::yield();
        }
        release.store(1, std::memory_order_release);
    });
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    const int submitted = sceAgcDriverSubmitDcb(&packet);
    if (submitted == 0) AgcDriverWaitIdle_nid_postfix();
    holder.join();
    Require(submitted == 0, "capture known label, a label the capture recorded: the submission failed");
    if (!recorded) {
        std::cout << "a label the capture recorded: the label was not recorded on the GPU, so nothing could hold it back\n";
        return 0;
    }
    Require(reached, "capture known label, a label the capture recorded: the label stayed in the open batch while another thread held the GPU mutex, letting only blocking callers through, and the queue waited behind it");
    std::cout << "a label the capture recorded reached memory while another thread held the GPU mutex, letting only blocking callers through\n";
    return 0;
}

struct HeldOutcome {
    bool skipped = false;
    int brightest = 0;
    std::uint64_t served = 0;
    std::uint64_t syncs = 0;
};

HeldOutcome RunBesideHeldWrite(const GuestBlock& block, const std::string& name, std::uint64_t labelOffset, bool knownLabels) {
    AgcDriver::Graphics::RuntimeSwitches::Assign(AgcDriver::Graphics::RuntimeSwitch::NoCaptureKnownLabels, !knownLabels);
    constexpr std::uint64_t releaseOffset = 0x8000;
    const auto label = std::bit_cast<std::uint32_t>(0.25f);
    const auto other = std::bit_cast<std::uint32_t>(0.5f);
    const auto otherOffset = labelOffset + sizeof(std::uint32_t);
    std::memset(block.Data() + labelOffset, 0, 2 * sizeof(std::uint32_t));
    std::memset(block.Data() + releaseOffset, 0, sizeof(std::uint32_t));
    std::memset(block.Data() + ColorOffset, 0, ColorBytes);
    CollectSetupStores(block);
    std::atomic_ref<std::uint32_t> landed(*reinterpret_cast<std::uint32_t*>(block.Data() + labelOffset));
    std::atomic_ref<std::uint32_t> otherWord(*reinterpret_cast<std::uint32_t*>(block.Data() + otherOffset));
    std::atomic_ref<std::uint32_t> release(*reinterpret_cast<std::uint32_t*>(block.Data() + releaseOffset));
    const std::string prefix = "capture known label, a label beside another queue's held write " + name + (knownLabels ? " with known labels" : " with the switch off");
    AgcDriver::Graphics::Recorder* recorder = nullptr;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        recorder = AgcDriver::Graphics::Recorder::Active();
    }
    Require(recorder != nullptr, prefix + ": no active recorder to hold a batch on");
    BatchHold hold(recorder->Device());
    std::uint64_t first = 0;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        hold.Arm(*recorder);
        first = recorder->OpenSerial();
    }
    std::vector<std::uint32_t> compute;
    AppendOpeningFill(compute, block.Address());
    AppendLabel(compute, block.Address() + otherOffset, other);
    Packet computePacket{compute.data(), static_cast<std::uint32_t>(compute.size()), 0, {}};
    if (sceAgcDriverSubmitAcb(0x20, &computePacket) != 0) {
        static_cast<void>(hold.Release());
        Require(false, prefix + ": the compute submission failed");
    }
    bool held = false;
    const auto heldDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool writtenByCpu = false;
    while (!held && !writtenByCpu && std::chrono::steady_clock::now() < heldDeadline) {
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            held = recorder->Submissions() >= first && !recorder->Recording();
        }
        writtenByCpu = otherWord.load(std::memory_order_acquire) == other;
        if (!held && !writtenByCpu) std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    if (!held || otherWord.load(std::memory_order_acquire) != 0) {
        static_cast<void>(hold.Release());
        AgcDriverWaitIdle_nid_postfix();
        Require(held || writtenByCpu, prefix + ": the compute queue's write was recorded but not submitted to the GPU");
        return HeldOutcome{true};
    }
    const auto knownBefore = AgcDriver::ShaderMemory::KnownValues();
    const auto syncsBefore = AgcDriver::Graphics::Recorder::HookSyncCount();
    bool synced = false;
    bool absentWhileWaiting = false;
    bool reached = false;
    VkResult released = VK_NOT_READY;
    std::thread helper([&] {
        auto& gpu = AgcDriver::GuestMemory::GpuMutex();
        const auto syncDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!synced && std::chrono::steady_clock::now() < syncDeadline) {
            {
                std::unique_lock probe(gpu, std::try_to_lock);
                if (probe.owns_lock()) synced = AgcDriver::Graphics::Recorder::HookSyncCount() != syncsBefore;
            }
            if (!synced) std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        absentWhileWaiting = synced && otherWord.load(std::memory_order_acquire) == 0;
        released = hold.Release();
        const auto holdDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!(reached = landed.load(std::memory_order_acquire) == label) && std::chrono::steady_clock::now() < holdDeadline) {
            {
                std::lock_guard holdLock(gpu);
                while (gpu.Waiters() == 0 && landed.load(std::memory_order_acquire) != label && std::chrono::steady_clock::now() < holdDeadline) std::this_thread::yield();
            }
            std::this_thread::yield();
        }
        release.store(1, std::memory_order_release);
    });
    std::vector<std::uint32_t> commands;
    AppendOpeningFill(commands, block.Address());
    AppendState(commands, block.Address() + ColorOffset, PairPixelCode.data());
    AppendConstantPointer(commands, block.Address() + labelOffset);
    AppendLabel(commands, block.Address() + labelOffset, label);
    AppendDraw(commands);
    AppendWait(commands, block.Address() + releaseOffset, 1);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    const int submitted = sceAgcDriverSubmitDcb(&packet);
    if (submitted == 0) AgcDriverWaitIdle_nid_postfix();
    else release.store(1, std::memory_order_release);
    helper.join();
    Require(submitted == 0, prefix + ": the submission failed");
    Require(released == VK_SUCCESS, prefix + ": the held batch could not be released");
    Require(synced, prefix + ": the capture did not wait for the word the held write covers, so it was served without its bytes");
    Require(absentWhileWaiting, prefix + ": the held write's bytes were already in memory when the capture waited for them");
    Require(reached, prefix + ": the capture's own label did not reach memory while another thread held the GPU mutex, letting only blocking callers through, and the queue waited behind it");
    HeldOutcome outcome;
    outcome.syncs = AgcDriver::Graphics::Recorder::HookSyncCount() - syncsBefore;
    outcome.served = AgcDriver::ShaderMemory::KnownValues().served - knownBefore.served;
    AgcDriver::GuestMemory::FlushGpuWrites(block.Address() + ColorOffset, ColorBytes);
    outcome.brightest = *std::max_element(block.Data() + ColorOffset, block.Data() + ColorOffset + ColorBytes);
    return outcome;
}

int CheckBesideHeldWrite(const GuestBlock& block, const std::string& name, std::uint64_t offOffset, std::uint64_t onOffset) {
    const auto before = RunBesideHeldWrite(block, name, offOffset, false);
    const auto after = RunBesideHeldWrite(block, name, onOffset, true);
    if (before.skipped || after.skipped) {
        std::cout << "a label beside another queue's held write " << name << ": the compute write was not recorded on the GPU, so nothing was pending" << '\n';
        return 0;
    }
    constexpr int expected = 191;
    Require(Near(before.brightest, expected), "capture known label, a label beside another queue's held write " + name + ": the draw wrote " + std::to_string(before.brightest) + " with the switch off, expected " + std::to_string(expected));
    Require(after.brightest == before.brightest, "capture known label, a label beside another queue's held write " + name + ": the draw wrote " + std::to_string(after.brightest) + " with known labels and " + std::to_string(before.brightest) + " with the switch off");
    if (LabelWordWatched(block, onOffset)) Require(after.served > before.served, "capture known label, a label beside another queue's held write " + name + ": the label word was not served from the label (" + std::to_string(after.served) + " served, " + std::to_string(before.served) + " with the switch off)");
    else std::cout << "a label beside another queue's held write " << name << ": the label's memory is compared after its host import instead of write-watched, so the label word is not served" << '\n';
    std::cout << "a label beside another queue's held write " << name << ": " << after.syncs << " hook syncs and " << after.served << " served words with known labels, " << before.syncs << " and " << before.served << " with the switch off" << '\n';
    return 0;
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "0");
#else
    setenv("APS5_PIPELINED_DRAWS", "0", 1);
#endif
#ifdef _WIN32
    _putenv_s("APS5_LABEL_FLUSH_US", "60000000");
#else
    setenv("APS5_LABEL_FLUSH_US", "60000000", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        const GuestBlock block;
        if (!block.Valid()) {
            std::puts("skipped, guest memory is not write-watched here");
            AgcDriverShutdown_nid_postfix();
            return VulkanTestSkipped;
        }
        Register(VertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
        Register(PixelHeader, ConstantPixelCode.data(), sizeof(ConstantPixelCode), 1);
        Register(PairPixelHeader, PairPixelCode.data(), sizeof(PairPixelCode), 1);
        int result = Check(block, "the label word itself", {0x1000, 0.25f, 0x1000, 1.0f, false, 0.0f}, {0x2000, 0.25f, 0x2000, 1.0f, false, 0.0f}, 255, true, true);
        if (result == 0) result = Check(block, "a word beside the label", {0x3000, 0.5f, 0x3040, 1.0f, false, 0.0f}, {0x4000, 0.5f, 0x4040, 1.0f, false, 0.0f}, 128, false, true);
        if (result == 0) result = Check(block, "a label a later fill overwrites", {0x5000, 0.25f, 0x5000, 1.0f, true, 0.5f}, {0x6000, 0.25f, 0x6000, 1.0f, true, 0.5f}, 128, false, false);
        if (result == 0) result = CheckSubmittedBeforeWait(block);
        if (result == 0) result = CheckBesideHeldWrite(block, "on one page", 0x9000, 0xa000);
        if (result == 0) result = CheckBesideHeldWrite(block, "across a page boundary", 0xbffc, 0xcffc);
        AgcDriver::Graphics::RuntimeSwitches::Assign(AgcDriver::Graphics::RuntimeSwitch::NoCaptureKnownLabels, false);
        AgcDriverShutdown_nid_postfix();
        if (result == 0) std::puts("capture known label tests passed");
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
