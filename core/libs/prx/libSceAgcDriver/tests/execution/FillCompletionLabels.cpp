#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t FillOffset = 4096;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "cannot allocate the fill test block");
        std::memset(block, 0, BlockBytes);
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

    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }
    const std::uint32_t* Words() const { return block; }

private:
    std::uint32_t* block = nullptr;
};

void CheckFill(const GuestBlock& block, std::size_t bytes, const std::array<std::uint32_t, 4>& pattern) {
    for (std::size_t i = 0; i < bytes / 4; ++i) {
        Require(block.Words()[FillOffset / 4 + i] == pattern[i % 4], "the completed fill contains stale label bytes");
    }
}

void Prepare(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    const std::array<std::uint32_t, 4> zero{};
    Require(device.FillBuffer(block.Address(), BlockBytes, zero), "the device must import the fill test block");
    device.WaitIdle();
    Require(device.FillBuffer(block.Address() + BlockBytes - 16, 16, zero), "the preceding GPU work was not recorded");
}

void GpuLabel(AgcDriver::VulkanDevice& device, const GuestBlock& block, std::size_t fillBytes, const std::array<std::uint32_t, 4>& pattern, std::size_t labelBytes, std::int64_t offset, bool submitted) {
    Prepare(device, block);
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "the Vulkan recorder is not active");
    const auto fill = block.Address() + FillOffset;
    const auto address = static_cast<std::uint64_t>(static_cast<std::int64_t>(fill) + offset);
    const std::vector<std::uint32_t> label(labelBytes / 4, 0x5a5a5a5au);
    Require(device.WriteLabelOnGpu(address, std::as_bytes(std::span(label)), 10, 0, false) == 0, "the label was not stored on the GPU");
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    if (submitted) device.SubmitRecorded(false);
    const auto ran = std::make_shared<bool>(false);
    Require(device.AfterRecordedWork([ran] { *ran = true; }, false), "the unrelated completion was not queued");
    Require(Recorder::PendingCompletionLabels() != 0 && recorder->PendingLabelIn(address, labelBytes), "the label and unrelated completion are not pending");
    Require(!recorder->CompletionLabelIn(address, labelBytes), "a GPU label requires a CPU completion");
    if (labelBytes <= Recorder::LabelTableBytes) {
        Recorder::LabelRefusal refusal{};
        const auto before = Recorder::LookupLabel(address, 4, 10, &refusal);
        Require(Recorder::LateTrust() ? before && before->late && before->value == label[0] : !before && refusal == Recorder::LabelRefusal::TrustOff, "the GPU label's initial late lookup is wrong");
    }
    Require(device.FillBuffer(fill, fillBytes, pattern), "the fill over the GPU label was not recorded");
    Require(!*ran, "the fill drained an unrelated CPU completion because of a GPU label");
    const bool overlaps = address < fill + fillBytes && address + labelBytes > fill;
    if (labelBytes <= Recorder::LabelTableBytes) {
        const auto probe = std::max(address, fill);
        Recorder::LabelRefusal refusal{};
        const auto hit = Recorder::LookupLabel(overlaps ? probe : address, 4, 10, &refusal);
        if (!Recorder::LateTrust()) {
            Require(!hit && refusal == Recorder::LabelRefusal::TrustOff, "the fill enabled a disabled late label shortcut");
        } else {
            Require(overlaps ? !hit && refusal == Recorder::LabelRefusal::Overwritten : hit && hit->value == label[0], "a later submission trusted an overwritten label or lost a disjoint one");
        }
        const auto early = Recorder::LookupLabel(address, 4, 9);
        Require(early && !early->late && early->value == label[0], "the fill lost the earlier submission's ordered label");
        if (labelBytes >= 8) {
            const auto pair = Recorder::LookupLabel(address, 8, 10, &refusal);
            if (!Recorder::LateTrust()) {
                Require(!pair && refusal == Recorder::LabelRefusal::TrustOff, "a composed label enabled a disabled shortcut");
            } else {
                Require(overlaps ? !pair && refusal == Recorder::LabelRefusal::Overwritten : pair && pair->value == 0x5a5a5a5a5a5a5a5aull, "a composed label trusted bytes overwritten by the fill");
            }
        }
    }
    device.WaitIdle();
    Require(*ran && Recorder::PendingCompletionLabels() == 0, "the unrelated completion did not retire");
    CheckFill(block, fillBytes, pattern);
    for (std::size_t i = 0; i < label.size(); ++i) {
        const auto word = address + i * 4;
        if (word < fill || word >= fill + fillBytes) {
            Require(block.Words()[(word - block.Address()) / 4] == label[i], "the fill changed a label outside its range");
        }
    }
    Require(!Recorder::LookupLabel(address, 4, 9), "a label outlived its completed batch");
}

void CpuLabel(AgcDriver::VulkanDevice& device, const GuestBlock& block, const std::array<std::uint32_t, 4>& pattern, bool overlaps) {
    Prepare(device, block);
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "the Vulkan recorder is not active");
    const auto fill = block.Address() + FillOffset;
    const std::uint32_t value = 0x5a5a5a5au;
    const auto bytes = std::as_bytes(std::span(&value, 1));
    Require(device.WriteLabelOnGpu(fill, bytes, 10, 0, false) == 0, "the preceding GPU label was not recorded");
    const auto address = overlaps ? fill + 12 : fill - 4;
    recorder->AfterCompletions(address, bytes, 11, 0, false);
    const auto ran = std::make_shared<bool>(false);
    Require(device.AfterRecordedWork([ran] { *ran = true; }, false), "the completion observer was not queued");
    Require(recorder->CompletionLabelIn(address, 4), "the CPU label is not pending");
    Require(device.FillBuffer(fill, 4096, pattern), "the fill following the CPU label was not recorded");
    Require(*ran == overlaps, "the fill did not wait exactly for an overlapping CPU completion");
    device.WaitIdle();
    CheckFill(block, 4096, pattern);
    if (!overlaps) Require(block.Words()[FillOffset / 4 - 1] == value, "the disjoint CPU label did not land");
}

}

int main(int argc, char** argv) {
    try {
        GuestBlock block;
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const std::array<std::uint32_t, 4> uniform{0x12345678u, 0x12345678u, 0x12345678u, 0x12345678u};
        const std::array<std::uint32_t, 4> pattern{0x11223344u, 0x55667788u, 0x99aabbccu, 0xddeeff00u};
        const auto mode = argc == 2 ? std::string_view(argv[1]) : std::string_view{};
        Require(mode.empty() || mode == "--submitted" || mode == "--cpu-overlap" || mode == "--cpu-disjoint", "unknown fill test mode");
        for (const auto& fill : {uniform, pattern}) {
            if (mode == "--cpu-overlap" || mode == "--cpu-disjoint") {
                CpuLabel(*device, block, fill, mode == "--cpu-overlap");
            } else {
                for (const auto bytes : {16u, 4096u}) {
                    for (const auto labelBytes : {4u, 8u, 64u, 256u}) {
                        GpuLabel(*device, block, bytes, fill, labelBytes, 0, mode == "--submitted");
                        GpuLabel(*device, block, bytes, fill, labelBytes, -4, mode == "--submitted");
                        GpuLabel(*device, block, bytes, fill, labelBytes, bytes - 4, mode == "--submitted");
                        GpuLabel(*device, block, bytes, fill, labelBytes, bytes, mode == "--submitted");
                    }
                }
            }
        }
        std::cout << "fill completion label tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
