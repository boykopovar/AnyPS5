#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <SDL_loadso.h>
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

alignas(256) std::array<std::uint32_t, 7> AddressWriterCode{0x7e000202u, 0x7e020203u, 0x7e040204u, 0x7e060280u, 0xdc7c8000u, 0x00000003u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 7> CounterCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e000281u, 0xe0c80000u, 0x80020000u, 0xbf810000u};

constexpr std::uint32_t ThreadDimensions = 0x8061u;
constexpr std::uint32_t GroupCounts = 0x8041u;
constexpr std::uint32_t Compute = 0x20u;
constexpr std::size_t GuestBytes = 1u << 20u;

struct Slot {
    alignas(256) std::array<std::uint32_t, 4> arguments{};
    alignas(256) std::array<std::uint32_t, 4> counter{};
    alignas(256) std::array<std::uint32_t, 4> counterDescriptor{};
    alignas(256) std::uint32_t done = 0;
};

struct Shape {
    const char* name;
    std::array<std::uint32_t, 3> threads;
    std::array<std::uint32_t, 3> group;
};

constexpr std::array<Shape, 4> Shapes{{
    {"100x1x1 threads in groups of 64x1x1", {100u, 1u, 1u}, {64u, 1u, 1u}},
    {"5x3x2 threads in groups of 4x2x2", {5u, 3u, 2u}, {4u, 2u, 2u}},
    {"64x2x1 threads in groups of 32x1x1", {64u, 2u, 1u}, {32u, 1u, 1u}},
    {"0x7x1 threads in groups of 8x1x1", {0u, 7u, 1u}, {8u, 1u, 1u}},
}};

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

std::array<std::uint32_t, 4> Descriptor(const void* pointer, std::uint32_t bytes) {
    return {Low(pointer), High(pointer) & 0xffffu, bytes, 0x31016facu};
}

std::uint32_t Load(const std::uint32_t& value) {
    return reinterpret_cast<const volatile std::uint32_t&>(value);
}

std::uint32_t Expected(const Shape& shape) {
    return shape.threads[0] * shape.threads[1] * shape.threads[2];
}

void SetShaderRegister(std::vector<std::uint32_t>& words, std::uint32_t offset, std::uint32_t value) {
    words.insert(words.end(), {0xc0017600u, offset, value});
}

void Program(std::vector<std::uint32_t>& words, const void* code, std::array<std::uint32_t, 3> group, std::initializer_list<std::uint32_t> userData) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(code));
    SetShaderRegister(words, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    SetShaderRegister(words, 0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (std::uint32_t axis = 0; axis < 3; ++axis) SetShaderRegister(words, 0x207u + axis, group[axis]);
    SetShaderRegister(words, 0x212, 0);
    SetShaderRegister(words, 0x213, static_cast<std::uint32_t>(userData.size()) << 1u);
    std::uint32_t offset = 0x240;
    for (const auto value : userData) SetShaderRegister(words, offset++, value);
}

void Reset(Slot& slot) {
    slot.arguments = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0};
    slot.counter = {};
    slot.counterDescriptor = Descriptor(slot.counter.data(), sizeof(slot.counter));
    slot.done = 0;
}

void Check(const Slot& slot, const Shape& shape, const std::string& name) {
    const auto start = std::chrono::steady_clock::now();
    while (Load(slot.done) == 0) {
        Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(10), name + ": the completion label did not land");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    Require(Load(slot.counter[0]) == Expected(shape), name + ": " + std::to_string(Load(slot.counter[0])) + " threads ran, expected " + std::to_string(Expected(shape)));
}

void RunSubmitted(Slot& slot, const Shape& shape, bool producedOnGpu) {
    Reset(slot);
    if (!producedOnGpu) {
        for (std::size_t axis = 0; axis < 3; ++axis) slot.arguments[axis] = shape.threads[axis];
    }
    std::vector<std::uint32_t> words;
    if (producedOnGpu) {
        Program(words, AddressWriterCode.data(), {1u, 1u, 1u}, {Low(slot.arguments.data()), High(slot.arguments.data()), shape.threads[0], shape.threads[1], shape.threads[2]});
        words.insert(words.end(), {0xc0031500u, 1, 1, 1, GroupCounts});
    }
    Program(words, CounterCode.data(), shape.group, {Low(slot.counterDescriptor.data()), High(slot.counterDescriptor.data())});
    words.insert(words.end(), {0xc0021102u, 1u, Low(slot.arguments.data()), High(slot.arguments.data())});
    words.insert(words.end(), {0xc0011600u, 0u, ThreadDimensions});
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(&slot.done), High(&slot.done), 1u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(Compute, &packet) == 0, "compute submit failed");
    AgcDriverWaitIdle_nid_postfix();
    Check(slot, shape, std::string(shape.name) + (producedOnGpu ? ", arguments stored by the GPU" : ", arguments stored by the CPU"));
}

ShaderRecompiler::RecompileResult Recompile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, std::span<const ShaderRecompiler::MemoryRegion> memory, std::array<std::uint32_t, 3> group, std::array<std::uint32_t, 3> partialThreads, std::uint32_t pushBytes) {
    const ShaderRecompiler::ShaderComputeStageInfo compute{group, 0, {false, false, false}, false, 1, partialThreads};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.ComputeTarget(32),
        {0, 0, 0, pushBytes}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void RunCpuFallback(Slot& slot, const Shape& shape) {
    AgcDriverShutdown_nid_postfix();
    Reset(slot);
    const auto device = OpenVulkanTestDevice();
    Require(device != nullptr, "cannot open the CPU fallback test device");
    const std::array<std::uint32_t, 5> producerData{Low(slot.arguments.data()), High(slot.arguments.data()), shape.threads[0], shape.threads[1], shape.threads[2]};
    const std::array<ShaderRecompiler::MemoryRegion, 1> producerMemory{{{reinterpret_cast<std::uintptr_t>(AddressWriterCode.data()), std::as_bytes(std::span(AddressWriterCode))}}};
    const auto producer = Recompile(*device, AddressWriterCode, producerData, producerMemory, {1u, 1u, 1u}, {}, 128u);
    const std::array<std::uint32_t, 2> consumerData{Low(slot.counterDescriptor.data()), High(slot.counterDescriptor.data())};
    const std::array<ShaderRecompiler::MemoryRegion, 2> consumerMemory{{
        {reinterpret_cast<std::uintptr_t>(CounterCode.data()), std::as_bytes(std::span(CounterCode))},
        {reinterpret_cast<std::uintptr_t>(slot.counterDescriptor.data()), std::as_bytes(std::span(slot.counterDescriptor))},
    }};
    const auto consumer = Recompile(*device, CounterCode, consumerData, consumerMemory, shape.group, {~0u, ~0u, ~0u}, 0u);
    const auto preparedProducer = device->PrepareDispatch(producer, {}, 1, reinterpret_cast<std::uintptr_t>(AddressWriterCode.data()));
    const auto preparedConsumer = device->PrepareDispatch(consumer, {}, 0, reinterpret_cast<std::uintptr_t>(CounterCode.data()));
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "the CPU fallback test has no active recorder");
    device->Dispatch(producer, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AddressWriterCode.data()), preparedProducer);
    const auto labelAddress = reinterpret_cast<std::uintptr_t>(&slot.arguments[2]);
    Require(device->WriteLabelOnGpu(labelAddress, std::as_bytes(std::span(&shape.threads[2], 1)), 1, Compute, false) == 0, "the argument label was not recorded on the GPU");
    const auto outcome = device->DispatchIndirect(consumer, reinterpret_cast<std::uintptr_t>(slot.arguments.data()), {}, reinterpret_cast<std::uintptr_t>(CounterCode.data()), preparedConsumer, nullptr, shape.group);
    Require(outcome.cpuReason == 2, std::string(shape.name) + ": the indirect dispatch did not take the pending-label CPU fallback (reason " + std::to_string(outcome.cpuReason) + ")");
    recorder->Sync();
    slot.done = 1;
    Check(slot, shape, std::string(shape.name) + ", read on the CPU behind a pending argument label");
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
#ifdef _WIN32
        auto* block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, GuestBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        auto* block = static_cast<std::uint8_t*>(std::aligned_alloc(GuestBytes, GuestBytes));
#endif
        Require(block != nullptr, "cannot allocate guest memory");
        GuestAllocations::Mutation().Add(block, GuestBytes, true, true, true);
        auto* slot = new (block) Slot{};
        const bool gpuThreadDimensions = std::getenv("APS5_NO_GPU_THREAD_DIMENSIONS") == nullptr;
        for (const auto& shape : Shapes) {
            if (gpuThreadDimensions) RunSubmitted(*slot, shape, true);
            RunSubmitted(*slot, shape, false);
            std::cout << shape.name << ": passed\n";
        }
        if (gpuThreadDimensions) {
            for (const auto& shape : Shapes) {
                RunCpuFallback(*slot, shape);
                std::cout << shape.name << ", CPU fallback: passed\n";
            }
        }
        std::cout << "indirect thread dimension tests passed\n";
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
