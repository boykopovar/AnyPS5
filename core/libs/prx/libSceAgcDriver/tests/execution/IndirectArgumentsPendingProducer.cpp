#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <SDL_loadso.h>
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
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
#include <new>
#include <memory>
#include <atomic>
#include <mutex>
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
alignas(256) std::array<std::uint32_t, 15> SpinCode{0xf4080200u, 0xfa000000u, 0xbe840381u, 0x9304ff04u, 0x0019660du, 0x8004ff04u, 0x3c6ef35fu, 0x80828102u, 0xbf078002u, 0xbf85fff9u, 0xbf8cc07fu, 0x7e000204u, 0xe0700000u, 0x80020000u, 0xbf810000u};

constexpr std::uint32_t Groups = 5;
constexpr std::array<std::uint32_t, 3> Stale{3u, 288u, 0xffffffffu};
constexpr std::uint32_t SpinIterations = 1u << 20u;
constexpr std::uint32_t SpinDispatches = 16;
constexpr std::uint32_t ThreadDimensions = 0x8061u;
constexpr std::uint32_t GroupCounts = 0x8041u;
constexpr std::uint32_t Compute = 0x20u;
constexpr std::uint32_t OtherCompute = 0x28u;
constexpr std::uint32_t Graphics = 0u;
constexpr std::size_t GuestBytes = 1u << 20u;

struct Slot {
    alignas(256) std::array<std::uint32_t, 4> arguments{};
    alignas(256) std::array<std::uint32_t, 4> counter{};
    alignas(256) std::array<std::uint32_t, 4> counterDescriptor{};
    alignas(256) std::array<std::uint32_t, 4> spin{};
    alignas(256) std::array<std::uint32_t, 4> spinDescriptor{};
    alignas(256) std::uint32_t label = 0;
    alignas(256) std::uint32_t done = 0;
};

enum class Order { SeparateSubmissions, OneSubmission, LabelWaitInOneSubmission, LabelWaitBetweenSubmissions, LabelWaitAcrossQueues, ArgumentLabelInEarlierBatch };

struct Variant {
    const char* name;
    std::uint32_t initiator;
    Order order;
};

constexpr std::array<Variant, 6> Variants{{
    {"thread dimensions, separate submissions", ThreadDimensions, Order::SeparateSubmissions},
    {"thread dimensions, one submission", ThreadDimensions, Order::OneSubmission},
    {"thread dimensions, label wait in one submission", ThreadDimensions, Order::LabelWaitInOneSubmission},
    {"thread dimensions, label wait between submissions", ThreadDimensions, Order::LabelWaitBetweenSubmissions},
    {"thread dimensions, label wait across compute queues", ThreadDimensions, Order::LabelWaitAcrossQueues},
    {"group counts read on the CPU behind an argument label of an earlier graphics batch", GroupCounts, Order::ArgumentLabelInEarlierBatch},
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

void SetShaderRegister(std::vector<std::uint32_t>& words, std::uint32_t offset, std::uint32_t value) {
    words.insert(words.end(), {0xc0017600u, offset, value});
}

void Program(std::vector<std::uint32_t>& words, const void* code, std::initializer_list<std::uint32_t> userData) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(code));
    SetShaderRegister(words, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    SetShaderRegister(words, 0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (const std::uint32_t offset : {0x207u, 0x208u, 0x209u}) SetShaderRegister(words, offset, 1);
    SetShaderRegister(words, 0x212, 0);
    SetShaderRegister(words, 0x213, static_cast<std::uint32_t>(userData.size()) << 1u);
    std::uint32_t offset = 0x240;
    for (const auto value : userData) SetShaderRegister(words, offset++, value);
}

void WriteData(std::vector<std::uint32_t>& words, std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(target), High(target), value});
}

void WaitEqual(std::vector<std::uint32_t>& words, std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0053c00u, 0x13u, Low(target), High(target), value, 0xffffffffu, 0x19u});
}

void Spin(std::vector<std::uint32_t>& words, Slot& slot, std::uint32_t dispatches) {
    for (std::uint32_t index = 0; index < dispatches; ++index) {
        Program(words, SpinCode.data(), {Low(slot.spinDescriptor.data()), High(slot.spinDescriptor.data()), SpinIterations});
        words.insert(words.end(), {0xc0031500u, 1, 1, 1, GroupCounts});
    }
}

void Produce(std::vector<std::uint32_t>& words, Slot& slot) {
    Program(words, AddressWriterCode.data(), {Low(slot.arguments.data()), High(slot.arguments.data()), Groups, 1u, 1u});
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, GroupCounts});
}

void Consume(std::vector<std::uint32_t>& words, Slot& slot, std::uint32_t initiator) {
    Program(words, CounterCode.data(), {Low(slot.counterDescriptor.data()), High(slot.counterDescriptor.data())});
    words.insert(words.end(), {0xc0021102u, 1u, Low(slot.arguments.data()), High(slot.arguments.data())});
    words.insert(words.end(), {0xc0011600u, 0u, initiator});
}

void Submit(std::uint32_t queue, std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    if (queue == Graphics) Require(sceAgcDriverSubmitDcb(&packet) == 0, "graphics submit failed");
    else Require(sceAgcDriverSubmitAcb(queue, &packet) == 0, "compute submit failed");
}

ShaderRecompiler::RecompileResult Recompile(AgcDriver::VulkanDevice& device, Slot& slot, std::span<const std::uint32_t> code, std::initializer_list<std::uint32_t> userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{
        {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
        {reinterpret_cast<std::uintptr_t>(slot.counterDescriptor.data()), std::as_bytes(std::span(slot.counterDescriptor))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, std::span(userData.begin(), userData.size()), compute, std::nullopt, std::nullopt, memory},
        device.ComputeTarget(32),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

class ProducerSubmissionGate {
public:
    explicit ProducerSubmissionGate(AgcDriver::VulkanDevice& device) : device(device) {
#ifdef _WIN32
        constexpr const char* libraryName = "vulkan-1.dll";
#elif defined(__APPLE__)
        constexpr const char* libraryName = "libvulkan.1.dylib";
#else
        constexpr const char* libraryName = "libvulkan.so.1";
#endif
        library.reset(SDL_LoadObject(libraryName));
        Require(library != nullptr, "cannot load Vulkan for the producer submission gate");
        resolve = reinterpret_cast<PFN_vkGetDeviceProcAddr>(SDL_LoadFunction(library.get(), "vkGetDeviceProcAddr"));
        Require(resolve != nullptr, "cannot resolve vkGetDeviceProcAddr for the producer submission gate");
        const auto createSemaphore = function<PFN_vkCreateSemaphore>("vkCreateSemaphore");
        destroySemaphore = function<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        signalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(resolve(device.Device(), "vkSignalSemaphoreKHR"));
        if (signalSemaphore == nullptr) signalSemaphore = function<PFN_vkSignalSemaphore>("vkSignalSemaphore");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
        Require(createSemaphore(device.Device(), &info, nullptr, &semaphore) == VK_SUCCESS, "cannot create the producer submission gate");
    }

    ~ProducerSubmissionGate() {
        if (worker.joinable()) {
            worker.request_stop();
            worker.join();
        }
        Release();
        try {
            device.WaitIdle();
        } catch (...) {
        }
        destroySemaphore(device.Device(), semaphore, nullptr);
    }

    void Arm(AgcDriver::Graphics::Recorder& recorder) {
        recorder.Commands();
        recorder.HoldNextSubmit(semaphore, 1);
    }

    void ReleaseAfterSubmission(AgcDriver::Graphics::Recorder& recorder, std::uint64_t serial) {
        worker = std::jthread([this, &recorder, serial](std::stop_token stop) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!stop.stop_requested() && recorder.NewestSubmitted() < serial && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::microseconds(100));
            producerSubmitted = recorder.NewestSubmitted() >= serial;
            Release();
            if (!producerSubmitted && !stop.stop_requested()) std::fprintf(stderr, "pending producer gate: watchdog expired with producer batch %llu unsubmitted (vkSignalSemaphore %d)\n", static_cast<unsigned long long>(serial), static_cast<int>(releaseResult));
        });
    }

    void Finish() {
        worker.join();
        Require(releaseResult == VK_SUCCESS, "cannot release the producer submission gate");
        Require(producerSubmitted, "the CPU argument read did not submit the later leased producer before waiting for its earlier argument label");
        device.WaitIdle();
    }

private:
    template <typename TFunction>
    TFunction function(const char* name) const {
        const auto function = reinterpret_cast<TFunction>(resolve(device.Device(), name));
        Require(function != nullptr, std::string("cannot resolve ") + name + " for the producer submission gate");
        return function;
    }

    void Release() {
        if (signaled.exchange(true)) return;
        VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
        info.semaphore = semaphore;
        info.value = 1;
        releaseResult = signalSemaphore(device.Device(), &info);
    }

    AgcDriver::VulkanDevice& device;
    std::unique_ptr<void, decltype(&SDL_UnloadObject)> library{nullptr, &SDL_UnloadObject};
    PFN_vkGetDeviceProcAddr resolve = nullptr;
    PFN_vkDestroySemaphore destroySemaphore = nullptr;
    PFN_vkSignalSemaphore signalSemaphore = nullptr;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    std::jthread worker;
    std::atomic<bool> signaled{false};
    bool producerSubmitted = false;
    VkResult releaseResult = VK_NOT_READY;
};

void RunArgumentLabel(Slot& slot) {
    AgcDriverShutdown_nid_postfix();
    slot.arguments = {3, 1, 1, 0};
    const auto device = OpenVulkanTestDevice();
    Require(device != nullptr, "cannot open the argument-label test device");
    const auto producer = Recompile(*device, slot, AddressWriterCode, {Low(slot.arguments.data()), High(slot.arguments.data()), Groups, 1, 1});
    const auto consumer = Recompile(*device, slot, CounterCode, {Low(slot.counterDescriptor.data()), High(slot.counterDescriptor.data())});
    const auto preparedProducer = device->PrepareDispatch(producer, {}, 1, reinterpret_cast<std::uintptr_t>(AddressWriterCode.data()));
    const auto preparedConsumer = device->PrepareDispatch(consumer, {}, 0, reinterpret_cast<std::uintptr_t>(CounterCode.data()));
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "the argument-label test has no active recorder");
    ProducerSubmissionGate gate(*device);
    gate.Arm(*recorder);
    const std::uint32_t labelValue = 2;
    const auto labelAddress = reinterpret_cast<std::uintptr_t>(&slot.arguments[1]);
    Require(device->WriteLabelOnGpu(labelAddress, std::as_bytes(std::span(&labelValue, 1)), 1, Graphics, false) == 0, "the argument label was not recorded on the GPU");
    device->SubmitRecorded(false);
    const auto producerSerial = recorder->OpenSerial();
    gate.ReleaseAfterSubmission(*recorder, producerSerial);
    const auto deferredBefore = AgcDriver::Graphics::LeaseCounters().deferred;
    device->Dispatch(producer, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AddressWriterCode.data()), preparedProducer);
    Require(AgcDriver::Graphics::LeaseCounters().deferred > deferredBefore, "the address-based producer did not defer its registry lease");
    Require(recorder->Submissions() < producerSerial && recorder->Recording(), "the producer was submitted before the CPU argument decision");
    Require(recorder->PendingLabel(labelAddress, 4, 0).has_value(), "the earlier argument label retired before the CPU argument decision");
    const auto outcome = device->DispatchIndirect(consumer, reinterpret_cast<std::uintptr_t>(slot.arguments.data()), {}, reinterpret_cast<std::uintptr_t>(CounterCode.data()), preparedConsumer);
    Require(outcome.cpuReason == 2, "the indirect dispatch did not take the pending-label CPU fallback (reason " + std::to_string(outcome.cpuReason) + ")");
    std::fprintf(stderr, "pending producer CPU fallback: cpuReason %d\n", outcome.cpuReason);
    gate.Finish();
    slot.done = 1;
}

void Run(Slot& slot, const Variant& variant) {
    slot.arguments = {Stale[0], Stale[1], Stale[2], 0};
    slot.counter = {};
    slot.counterDescriptor = Descriptor(slot.counter.data(), sizeof(slot.counter));
    slot.spinDescriptor = Descriptor(slot.spin.data(), sizeof(slot.spin));
    slot.label = 0;
    slot.done = 0;
    std::vector<std::uint32_t> first;
    std::vector<std::uint32_t> second;
    switch (variant.order) {
    case Order::SeparateSubmissions:
        Spin(first, slot, SpinDispatches);
        Produce(first, slot);
        Consume(second, slot, variant.initiator);
        WriteData(second, &slot.done, 1);
        Submit(Compute, first);
        Submit(Compute, second);
        break;
    case Order::OneSubmission:
        Spin(first, slot, SpinDispatches);
        Produce(first, slot);
        Consume(first, slot, variant.initiator);
        WriteData(first, &slot.done, 1);
        Submit(Compute, first);
        break;
    case Order::LabelWaitInOneSubmission:
        Spin(first, slot, SpinDispatches);
        Produce(first, slot);
        WriteData(first, &slot.label, 1);
        WaitEqual(first, &slot.label, 1);
        Consume(first, slot, variant.initiator);
        WriteData(first, &slot.done, 1);
        Submit(Compute, first);
        break;
    case Order::LabelWaitBetweenSubmissions:
        Spin(first, slot, SpinDispatches);
        Produce(first, slot);
        WriteData(first, &slot.label, 1);
        WaitEqual(second, &slot.label, 1);
        Consume(second, slot, variant.initiator);
        WriteData(second, &slot.done, 1);
        Submit(Compute, first);
        Submit(Compute, second);
        break;
    case Order::LabelWaitAcrossQueues:
        Spin(first, slot, SpinDispatches);
        Produce(first, slot);
        WriteData(first, &slot.label, 1);
        WaitEqual(second, &slot.label, 1);
        Consume(second, slot, variant.initiator);
        WriteData(second, &slot.done, 1);
        Submit(OtherCompute, first);
        Submit(Compute, second);
        break;
    case Order::ArgumentLabelInEarlierBatch:
        RunArgumentLabel(slot);
        break;
    }
    if (variant.order != Order::ArgumentLabelInEarlierBatch) AgcDriverWaitIdle_nid_postfix();
    const auto start = std::chrono::steady_clock::now();
    while (Load(slot.done) == 0) {
        Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(10), std::string(variant.name) + ": the consumer's completion label did not land");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    Require(Load(slot.arguments[0]) == Groups && Load(slot.arguments[1]) == 1 && Load(slot.arguments[2]) == 1, std::string(variant.name) + ": the producer's address-based store did not write the indirect arguments");
    Require(Load(slot.counter[0]) == Groups, std::string(variant.name) + ": the indirect dispatch ran " + std::to_string(Load(slot.counter[0])) + " threads, expected the " + std::to_string(Groups) + " its producer's address-based store wrote");
}

}

int main(int argc, char** argv) {
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
        auto* slots = new (block) Slot[Variants.size()];
        for (std::size_t index = 0; index < Variants.size(); ++index) {
            if (argc > 1 && std::to_string(index) != argv[1]) continue;
            Run(slots[index], Variants[index]);
            std::cout << Variants[index].name << ": passed\n";
        }
        std::cout << "indirect arguments pending producer tests passed\n";
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
