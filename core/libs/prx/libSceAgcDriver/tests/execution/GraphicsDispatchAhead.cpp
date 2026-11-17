#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <array>
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
alignas(256) std::array<std::uint32_t, 4> Counted{};
alignas(256) std::array<std::uint32_t, 4> CountedDescriptor{};
alignas(256) volatile std::uint32_t Done = 0;

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

void WriteData(std::vector<std::uint32_t>& words, const volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(target), High(target), value});
}

void Run(std::uint32_t serial) {
    CountedDescriptor = {Low(Counted.data()), High(Counted.data()) & 0xffffu, sizeof(Counted), 0x31016facu};
    std::vector<std::uint32_t> words;
    Dispatch(words, CounterCode.data(), {Low(CountedDescriptor.data()), High(CountedDescriptor.data())});
    Dispatch(words, CounterCode.data(), {Low(CountedDescriptor.data()), High(CountedDescriptor.data())});
    WriteData(words, &Done, serial);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (Done != serial) {
        Require(std::chrono::steady_clock::now() < deadline, "the graphics queue submission never finished");
        std::this_thread::yield();
    }
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Counted.data()), sizeof(Counted));
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        Run(1);
        Require(Counted[0] == 2, "the warm-up dispatches on the graphics queue did not run");
        const auto adopted = AgcDriverDispatchesAdoptedAhead_nid_postfix();
        Run(2);
        Require(Counted[0] == 4, "the second submission's dispatches on the graphics queue did not run");
        const auto adoptedNow = AgcDriverDispatchesAdoptedAhead_nid_postfix();
        std::cout << "graphics queue dispatches adopted ahead: " << adoptedNow - adopted << '\n';
        Require(adoptedNow > adopted, "with APS5_GROUP_CAPTURE_ALL_QUEUES and draw pipelining off, the second dispatch on the graphics queue was neither pipelined nor resolved ahead");
        std::cout << "graphics dispatch ahead test passed\n";
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
