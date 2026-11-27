#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

alignas(256) std::array<std::uint32_t, 12> ReaderCode{0xf4040100u, 0xfa000000u, 0xf4080201u, 0xfa000000u, 0xbf8cc07fu, 0xf4000182u, 0xfa000000u, 0xbf8cc07fu, 0x7e000206u, 0xe0700000u, 0x80020000u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 14> NewReaderCode{0xf4040100u, 0xfa000000u, 0xf4080201u, 0xfa000000u, 0xbf8cc07fu, 0xf4000182u, 0xfa000000u, 0xbf8cc07fu, 0x7e000206u, 0xe0700000u, 0x80020000u, 0xe0700004u, 0x80020000u, 0xbf810000u};

struct Slot {
    alignas(256) std::array<std::uint32_t, 4> value{};
    alignas(256) std::array<std::uint32_t, 2> table{};
    alignas(256) std::array<std::uint32_t, 4> output{};
    alignas(256) std::array<std::uint32_t, 4> outputDescriptor{};
    alignas(256) volatile std::uint32_t done = 0;
};

std::array<Slot, 3> Slots;

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

void SetShaderRegister(std::vector<std::uint32_t>& words, std::uint32_t offset, std::uint32_t value) {
    words.insert(words.end(), {0xc0017600u, offset, value});
}

void Dispatch(std::vector<std::uint32_t>& words, const void* code, std::initializer_list<std::uint32_t> userData) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(code));
    SetShaderRegister(words, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    SetShaderRegister(words, 0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (const std::uint32_t offset : {0x207u, 0x208u, 0x209u}) SetShaderRegister(words, offset, 1);
    SetShaderRegister(words, 0x212, 0);
    SetShaderRegister(words, 0x213, static_cast<std::uint32_t>(userData.size()) << 1u);
    std::uint32_t offset = 0x240;
    for (const auto value : userData) SetShaderRegister(words, offset++, value);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void WriteData(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(target), High(target), value});
}

alignas(256) volatile std::uint32_t Gate = 0;

void Prepare(Slot& slot, std::uint32_t value) {
    slot.value[0] = value;
    slot.table = {Low(slot.value.data()), High(slot.value.data())};
    slot.outputDescriptor = {Low(slot.output.data()), High(slot.output.data()) & 0xffffu, static_cast<std::uint32_t>(sizeof(slot.output)), 0x31016facu};
}

void DispatchSlot(std::vector<std::uint32_t>& words, Slot& slot, const void* code) {
    Dispatch(words, code, {Low(slot.table.data()), High(slot.table.data()), Low(slot.outputDescriptor.data()), High(slot.outputDescriptor.data())});
    WriteData(words, &slot.done, 1);
}

void WaitDone(const Slot& slot, const char* what) {
    const auto start = std::chrono::steady_clock::now();
    while (slot.done == 0) {
        Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(60), what);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

}

int main() {
    using AgcDriver::VulkanDevice;
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        const bool prebuild = std::getenv("APS5_NO_GRAPHICS_QUEUE_PIPELINE_PREBUILD") == nullptr;
        Prepare(Slots[2], 0x9abcu);
        std::vector<std::uint32_t> warm;
        DispatchSlot(warm, Slots[2], ReaderCode.data());
        Packet warmPacket{warm.data(), static_cast<std::uint32_t>(warm.size()), 0, {}};
        Require(sceAgcDriverSubmitAcb(0x28, &warmPacket) == 0, "the warm-up submit failed");
        WaitDone(Slots[2], "the warm-up dispatch did not finish");
        Require(Slots[2].output[0] == 0x9abcu, "the warm-up dispatch stored " + std::to_string(Slots[2].output[0]));
        Prepare(Slots[0], 0x1234u);
        Prepare(Slots[1], 0x5678u);
        std::vector<std::uint32_t> words;
        DispatchSlot(words, Slots[0], ReaderCode.data());
        words.insert(words.end(), {0xc0053c00u, 0x13u, Low(&Gate), High(&Gate), 1u, 0xffffffffu, 0x19u});
        DispatchSlot(words, Slots[1], NewReaderCode.data());
        const auto before = VulkanDevice::ComputePipelineCounts();
        const auto adopted = AgcDriverDispatchesAdoptedAhead_nid_postfix();
        Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the graphics queue submit failed");
        WaitDone(Slots[0], "the dispatch before the wait did not finish");
        Require(Slots[0].output[0] == 0x1234u, "the dispatch before the wait stored " + std::to_string(Slots[0].output[0]));
        const auto start = std::chrono::steady_clock::now();
        const auto limit = prebuild ? std::chrono::seconds(60) : std::chrono::seconds(2);
        while (VulkanDevice::ComputePipelineCounts().created < before.created + 1 && std::chrono::steady_clock::now() - start < limit) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const auto waiting = VulkanDevice::ComputePipelineCounts();
        Require(Slots[1].done == 0, "the dispatch behind the wait ran before the wait passed");
        if (prebuild) {
            Require(waiting.created == before.created + 1, "queue 0 did not build the pipeline of the dispatch behind a wait while the worker waited: " + std::to_string(waiting.created - before.created) + " pipelines created");
            Require(waiting.ahead > before.ahead, "the pipeline of the dispatch behind the wait was not built ahead");
        } else {
            Require(waiting.created == before.created, "queue 0 built " + std::to_string(waiting.created - before.created) + " pipelines before the wait passed with the prebuild turned off");
        }
        Gate = 1;
        WaitDone(Slots[1], "the dispatch behind the wait did not finish");
        Require(Slots[1].output[0] == 0x5678u && Slots[1].output[1] == 0x5678u, "the dispatch behind the wait stored " + std::to_string(Slots[1].output[0]) + " and " + std::to_string(Slots[1].output[1]));
        const auto after = VulkanDevice::ComputePipelineCounts();
        Require(after.created == before.created + 1, "the dispatch of the new program created " + std::to_string(after.created - before.created) + " pipelines");
        Require(AgcDriverDispatchesAdoptedAhead_nid_postfix() == adopted, "queue 0 adopted a dispatch resolved ahead");
        std::cout << "graphics queue pipeline prebuild test passed (prebuild " << (prebuild ? "on" : "off") << ")\n";
        AgcDriverWaitIdle_nid_postfix();
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        Gate = 1;
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
