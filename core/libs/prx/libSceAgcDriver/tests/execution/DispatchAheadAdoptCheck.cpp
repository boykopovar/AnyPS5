#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

alignas(256) std::array<std::uint32_t, 7> CounterCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e000281u, 0xe0c80000u, 0x80020000u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 7> StoreCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0xe0700000u, 0x80020000u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 15> SpinCode{0xf4080200u, 0xfa000000u, 0xbe840381u, 0x9304ff04u, 0x0019660du, 0x8004ff04u, 0x3c6ef35fu, 0x80828102u, 0xbf078002u, 0xbf85fff9u, 0xbf8cc07fu, 0x7e000204u, 0xe0700000u, 0x80020000u, 0xbf810000u};

constexpr std::uint32_t Compute = 0x20u;
constexpr std::uint32_t OtherCompute = 0x28u;
constexpr std::uint32_t SpinIterations = 1u << 20u;

alignas(256) std::array<std::uint32_t, 4> Stale{};
alignas(256) std::array<std::uint32_t, 4> Fresh{};
alignas(256) std::array<std::uint32_t, 4> Target{};
alignas(256) std::array<std::uint32_t, 4> TargetDescriptor{};
alignas(256) std::array<std::uint32_t, 4> Spun{};
alignas(256) std::array<std::uint32_t, 4> SpunDescriptor{};
alignas(256) volatile std::uint32_t Gate = 1;
alignas(256) volatile std::uint32_t Done = 0;
alignas(256) volatile std::uint32_t OtherDone = 0;

enum class Window { None, CpuStore, QueuedGpuStore };

std::atomic<Window> Armed{Window::None};
std::uint32_t SpinDispatches = 16;
std::vector<std::uint32_t> OtherWords;
bool SawPending = false;

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

std::array<std::uint32_t, 4> Descriptor(const void* pointer, std::uint32_t bytes) {
    return {Low(pointer), High(pointer) & 0xffffu, bytes, 0x31016facu};
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

void Dispatch(std::vector<std::uint32_t>& words, const void* code, std::initializer_list<std::uint32_t> userData) {
    Program(words, code, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void WriteData(std::vector<std::uint32_t>& words, const volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(target), High(target), value});
}

void Submit(std::uint32_t queue, std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(queue, &packet) == 0, "compute submit failed");
}

void QueueStoreAhead() {
    OtherWords.clear();
    for (std::uint32_t index = 0; index < SpinDispatches; ++index) Dispatch(OtherWords, SpinCode.data(), {Low(SpunDescriptor.data()), High(SpunDescriptor.data()), SpinIterations});
    Dispatch(OtherWords, StoreCode.data(), {Low(TargetDescriptor.data()), High(TargetDescriptor.data()), Low(Fresh.data())});
    WriteData(OtherWords, &OtherDone, 1);
    Submit(OtherCompute, OtherWords);
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5) && OtherDone == 0) {
        if (AgcDriver::DriverDetail::Driver::QueryPendingSpan(reinterpret_cast<std::uintptr_t>(Target.data()), sizeof(Target), {}) != AgcDriver::ShaderMemory::PendingWrite::None) {
            SawPending = true;
            return;
        }
        std::this_thread::yield();
    }
}

void BeforeAdoption() {
    const auto window = Armed.exchange(Window::None);
    if (window == Window::CpuStore) reinterpret_cast<volatile std::uint32_t&>(Target[0]) = Low(Fresh.data());
    else if (window == Window::QueuedGpuStore) QueueStoreAhead();
}

void WaitIdle() {
    AgcDriverWaitIdle_nid_postfix();
    for (const auto* buffer : {&Stale, &Fresh, &Target}) AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(buffer->data()), sizeof(*buffer));
}

void Reset() {
    WaitIdle();
    Stale = {};
    Fresh = {};
    Target = Descriptor(Stale.data(), sizeof(Stale));
    Done = 0;
    OtherDone = 0;
    SawPending = false;
}

void Gated(Window window) {
    Reset();
    std::vector<std::uint32_t> words;
    words.insert(words.end(), {0xc0053c00u, 0x13u, Low(&Gate), High(&Gate), 1u, 0xffffffffu, 0x19u});
    Dispatch(words, CounterCode.data(), {Low(Target.data()), High(Target.data())});
    WriteData(words, &Done, 1);
    Armed = window;
    Submit(Compute, words);
    WaitIdle();
    Armed = Window::None;
    Require(Done == 1, "the gated submission did not finish");
}

std::string Counts() {
    return "(stale " + std::to_string(Stale[0]) + ", fresh " + std::to_string(Fresh[0]) + ")";
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        Require(High(Stale.data()) == High(Fresh.data()), "the stale and fresh buffers differ in their high address word");
        TargetDescriptor = Descriptor(Target.data(), sizeof(Target));
        SpunDescriptor = Descriptor(Spun.data(), sizeof(Spun));
        AgcDriverSetAheadAdoptHook_nid_postfix(&BeforeAdoption);
        Reset();
        std::vector<std::uint32_t> warmUp;
        Dispatch(warmUp, CounterCode.data(), {Low(Target.data()), High(Target.data())});
        WriteData(warmUp, &Done, 1);
        Submit(Compute, warmUp);
        std::vector<std::uint32_t> otherWarmUp;
        Dispatch(otherWarmUp, SpinCode.data(), {Low(SpunDescriptor.data()), High(SpunDescriptor.data()), SpinIterations});
        Dispatch(otherWarmUp, StoreCode.data(), {Low(TargetDescriptor.data()), High(TargetDescriptor.data()), Low(Fresh.data())});
        Submit(OtherCompute, otherWarmUp);
        WaitIdle();
        Require(Done == 1 && Target[0] == Low(Fresh.data()), "the warm-up dispatches did not run");

        Gated(Window::None);
        Require(Stale[0] == 1 && Fresh[0] == 0, "the gated dispatch without a store in the window did not count into the stale buffer " + Counts());
        const auto adopted = AgcDriverDispatchesAdoptedAhead_nid_postfix();
        Require(adopted != 0, "the gated dispatch was not resolved ahead and adopted");

        auto rejected = AgcDriverDispatchesRejectedAhead_nid_postfix();
        Gated(Window::CpuStore);
        Require(Fresh[0] == 1 && Stale[0] == 0, "a dispatch resolved ahead whose descriptor the CPU replaced before its adoption used the old descriptor " + Counts());
        Require(AgcDriverDispatchesRejectedAhead_nid_postfix() == rejected + 1, "the CPU store in the adoption window was not caught by the adoption check");
        std::cout << "cpu store before adoption: passed\n";

        bool conclusive = false;
        for (int attempt = 0; attempt < 4 && !conclusive; ++attempt, SpinDispatches *= 4) {
            rejected = AgcDriverDispatchesRejectedAhead_nid_postfix();
            Gated(Window::QueuedGpuStore);
            conclusive = SawPending;
            if (!conclusive) {
                std::cout << "queued gpu store before adoption: the store finished before the adoption with " << SpinDispatches << " spin dispatches, retrying\n";
                continue;
            }
            Require(Fresh[0] == 1 && Stale[0] == 0, "a dispatch resolved ahead whose descriptor a GPU store queued on another queue replaces before its adoption used the old descriptor " + Counts());
            Require(AgcDriverDispatchesRejectedAhead_nid_postfix() == rejected + 1, "the queued GPU store in the adoption window was not caught by the adoption check");
        }
        AgcDriverSetAheadAdoptHook_nid_postfix(nullptr);
        if (!conclusive) {
            std::cout << "queued gpu store before adoption: never observed the store pending at the adoption\n";
            AgcDriverShutdown_nid_postfix();
            return VulkanTestSkipped;
        }
        std::cout << "queued gpu store before adoption: passed\n";
        std::cout << "dispatch ahead adopt check test passed\n";
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
