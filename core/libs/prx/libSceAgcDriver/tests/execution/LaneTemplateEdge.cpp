#include "VulkanTestDevice.hpp"
#include "LaneGate.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/AsyncCompute.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("lane template edge: " + message);
}

alignas(256) constexpr std::array<std::uint32_t, 5> IndexStoreCode{0x7e000204u, 0x34020082u, 0xe0701000u, 0x80000001u, 0xbf810000u};

constexpr std::size_t GuestBytes = 1u << 16u;

struct Guest {
    alignas(256) std::array<std::uint32_t, 4> output{};
};

std::uint32_t Low(const void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

ShaderRecompiler::RecompileResult Recompile(AgcDriver::VulkanDevice& device, Guest& guest) {
    const std::array<std::uint32_t, 5> userData{Low(guest.output.data()), High(guest.output.data()) & 0xffffu, static_cast<std::uint32_t>(sizeof(guest.output)), 0x31016facu, 1u};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{
        {reinterpret_cast<std::uintptr_t>(IndexStoreCode.data()), std::as_bytes(std::span(IndexStoreCode))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(IndexStoreCode.data()), std::span(IndexStoreCode), 0, {}},
        {32, 0, std::span(userData), compute, std::nullopt, std::nullopt, memory},
        device.ComputeTarget(32),
        {0, 0, 124, 4}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

using Gate = AgcDriver::Tests::TimelineGate;

std::uint32_t Load(const std::uint32_t& value) {
    return reinterpret_cast<const volatile std::uint32_t&>(value);
}

}

int main() {
    using AgcDriver::Graphics::Recorder;
#ifdef _WIN32
    _putenv_s("APS5_ASYNC_LANE", "51");
#else
    setenv("APS5_ASYNC_LANE", "51", 1);
#endif
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        {
            std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
            auto* recorder = Recorder::Active();
            Require(recorder != nullptr, "no active recorder");
            if (!recorder->AsyncLaneActive()) {
                std::puts("skipped: the device has no eligible compute-only queue for the async lane");
                lock.unlock();
                device.reset();
                AgcDriverShutdown_nid_postfix();
                return VulkanTestSkipped;
            }
        }
#ifdef _WIN32
        auto* block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, GuestBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        auto* block = static_cast<std::uint8_t*>(std::aligned_alloc(GuestBytes, GuestBytes));
#endif
        Require(block != nullptr, "cannot allocate guest memory");
        std::memset(block, 0, GuestBytes);
        GuestAllocations::Mutation().Add(block, GuestBytes, true, true, true);
        auto* guest = new (block) Guest{};
        const auto program = reinterpret_cast<std::uintptr_t>(IndexStoreCode.data());
        const auto first = Recompile(*device, *guest);
        Require(AgcDriver::Graphics::AsyncDispatchEligible(first), "template fixture is outside async dispatch admission");
        Require(first.pushConstants.empty(), "template fixture did not spill runtime words into ShaderData");
        const auto scalar = std::find(first.shaderDataUserWords.begin(), first.shaderDataUserWords.end(), 4u);
        Require(scalar != first.shaderDataUserWords.end(), "template fixture lost the runtime s4 value");
        const auto scalarIndex = static_cast<std::size_t>(scalar - first.shaderDataUserWords.begin());
        auto second = first;
        std::size_t changed = 0;
        for (auto& binding : second.bindings) {
            Require(binding.role != ShaderRecompiler::DescriptorRole::FlattenedSrt, "direct descriptor fixture unexpectedly uses an SRT");
            if (binding.role != ShaderRecompiler::DescriptorRole::ShaderData) continue;
            Require(scalarIndex < binding.guestDescriptor.size() && binding.guestDescriptor[scalarIndex] == 1u, "ShaderData does not hold the first runtime value");
            binding.guestDescriptor[scalarIndex] = 2u;
            ++changed;
        }
        Require(changed == 1 && AgcDriver::Graphics::AsyncDispatchEligible(second), "second fixture must change one admitted ShaderData binding");
        Require(AgcDriver::VulkanDevice::TemplateDataRefresh(), "template data refresh must be enabled for this fixture");
        const AgcDriver::Graphics::CompiledShader firstStage{ShaderRecompiler::ShaderStage::Compute, &first, 0};
        const AgcDriver::Graphics::CompiledShader secondStage{ShaderRecompiler::ShaderStage::Compute, &second, 0};
        const bool movable = std::getenv("APS5_NO_DISPATCH_MOVE") == nullptr;
        auto templateKey = AgcDriver::Graphics::ShaderResources::ContentKey(firstStage, false, movable);
        Require(templateKey == AgcDriver::Graphics::ShaderResources::ContentKey(secondStage, false, movable), "fixture dispatches have different template keys");
        Require(AgcDriver::Graphics::ShaderResources::ContentKey(firstStage, true) != AgcDriver::Graphics::ShaderResources::ContentKey(secondStage, true), "fixture dispatches have identical ShaderData");
        const auto deviceHandle = reinterpret_cast<std::uintptr_t>(device->Device());
        templateKey.push_back(static_cast<std::uint32_t>(deviceHandle));
        templateKey.push_back(static_cast<std::uint32_t>(deviceHandle >> 32u));
        {
            std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
            auto* recorder = Recorder::Active();
            recorder->Sync();
        }
        {
            for (int warm = 0; warm < 3; ++warm) {
                const auto& shader = warm == 1 ? second : first;
                const auto prepared = device->PrepareDispatch(shader, {}, 1, program);
                std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
                device->Dispatch(shader, 1, 1, 1, {}, program, prepared);
                Recorder::Active()->Sync();
            }
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(guest->output.data()), sizeof(guest->output));
            Require(Load(guest->output[1]) == 1u && Load(guest->output[2]) == 2u, "the warm-up dispatches did not store their values");
            std::shared_ptr<AgcDriver::Graphics::ShaderResources> sharedTemplate;
            {
                std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
                sharedTemplate = AgcDriver::Graphics::SharedResourceCache().Find(templateKey);
                Require(sharedTemplate != nullptr && sharedTemplate->Reusable(), "warm-up did not create a reusable shared template");
            }
            for (auto& word : guest->output) reinterpret_cast<volatile std::uint32_t&>(word) = 0;
            Gate gate(*device);
            const auto firstPrepared = device->PrepareDispatch(first, {}, 1, program);
            {
                std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
                auto* recorder = Recorder::Active();
                const Recorder::LaneScope async(1);
                gate.Record(recorder->Commands());
                device->Dispatch(first, 1, 1, 1, {}, program, firstPrepared);
                Require(AgcDriver::Graphics::SharedResourceCache().Find(templateKey) == sharedTemplate && sharedTemplate->DataWordsHash() == AgcDriver::Graphics::ShaderResources::DataWordsHash(firstStage), "the held dispatch did not reuse the first template words");
            }
            const auto secondPrepared = device->PrepareDispatch(second, {}, 1, program);
            {
                std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
                auto* recorder = Recorder::Active();
                device->Dispatch(second, 1, 1, 1, {}, program, secondPrepared);
                Require(AgcDriver::Graphics::SharedResourceCache().Find(templateKey) == sharedTemplate && sharedTemplate->DataWordsHash() == AgcDriver::Graphics::ShaderResources::DataWordsHash(secondStage), "the main dispatch did not refresh the same shared template");
                recorder->Submit();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            gate.Release();
            {
                std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
                Recorder::Active()->Sync();
            }
        }
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(guest->output.data()), sizeof(guest->output));
        const auto one = Load(guest->output[1]);
        const auto two = Load(guest->output[2]);
        std::printf("lane template edge: output %u %u %u %u\n", Load(guest->output[0]), one, two, Load(guest->output[3]));
        Require(two == 2u, "the main lane's dispatch stored " + std::to_string(two) + " at index 2, expected 2");
        Require(one == 1u, "the async lane's dispatch, recorded first, read the cached template's words after the main lane refreshed them: index 1 holds " + std::to_string(one) + ", expected 1");
        device.reset();
        std::puts("lane template edge tests passed");
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
