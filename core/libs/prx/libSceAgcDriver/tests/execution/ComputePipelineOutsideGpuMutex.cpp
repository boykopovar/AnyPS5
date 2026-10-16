#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include <array>
#include <chrono>
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

std::array<Slot, 2> Slots;

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

void Submit(std::uint32_t queue, Slot& slot, const void* code, std::uint32_t value) {
    slot.value[0] = value;
    slot.table = {Low(slot.value.data()), High(slot.value.data())};
    slot.outputDescriptor = {Low(slot.output.data()), High(slot.output.data()) & 0xffffu, static_cast<std::uint32_t>(sizeof(slot.output)), 0x31016facu};
    std::vector<std::uint32_t> words;
    Dispatch(words, code, {Low(slot.table.data()), High(slot.table.data()), Low(slot.outputDescriptor.data()), High(slot.outputDescriptor.data())});
    WriteData(words, &slot.done, 1);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(queue, &packet) == 0, "compute submit failed");
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
        Submit(0x28, Slots[0], ReaderCode.data(), 0x1234u);
        WaitDone(Slots[0], "the first dispatch did not finish");
        Require(Slots[0].output[0] == 0x1234u, "the first dispatch stored " + std::to_string(Slots[0].output[0]));

        const auto before = VulkanDevice::ComputePipelineCounts();
        {
            std::unique_lock gpuLock(AgcDriver::GuestMemory::GpuMutex());
            Submit(0x20, Slots[1], NewReaderCode.data(), 0x5678u);
            const auto start = std::chrono::steady_clock::now();
            while (VulkanDevice::ComputePipelineCounts().created == before.created) {
                Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(60), "a dispatch of a new program did not create its pipeline while another thread held the GPU mutex");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            Require(Slots[1].done == 0, "the dispatch finished while another thread held the GPU mutex");
        }
        WaitDone(Slots[1], "the dispatch of the new program did not finish");
        Require(Slots[1].output[0] == 0x5678u && Slots[1].output[1] == 0x5678u, "the dispatch of the new program stored " + std::to_string(Slots[1].output[0]) + " and " + std::to_string(Slots[1].output[1]));
        const auto after = VulkanDevice::ComputePipelineCounts();
        Require(after.underGpuMutex == before.underGpuMutex, "a compute pipeline was created under the GPU mutex");
        std::cout << "compute pipeline outside GPU mutex test passed\n";
        AgcDriverWaitIdle_nid_postfix();
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
