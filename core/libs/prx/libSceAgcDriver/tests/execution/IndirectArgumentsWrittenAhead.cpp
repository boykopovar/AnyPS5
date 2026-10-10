#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include <array>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

alignas(256) std::array<std::uint32_t, 9> WriterCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020203u, 0x7e040204u, 0xe07c0000u, 0x80020000u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 7> CounterCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e000281u, 0xe0c80000u, 0x80020000u, 0xbf810000u};

alignas(256) std::array<std::uint32_t, 4> Arguments{1, 1, 1, 0};
alignas(256) std::array<std::uint32_t, 4> ArgumentsDescriptor{};
alignas(256) std::array<std::uint32_t, 4> Counter{};
alignas(256) std::array<std::uint32_t, 4> CounterDescriptor{};
alignas(256) volatile std::uint32_t Gate = 1;
alignas(256) volatile std::uint32_t Done = 0;

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

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        ArgumentsDescriptor = Descriptor(Arguments.data(), sizeof(Arguments));
        CounterDescriptor = Descriptor(Counter.data(), sizeof(Counter));
        std::vector<std::uint32_t> warmUp;
        Program(warmUp, CounterCode.data(), {Low(CounterDescriptor.data()), High(CounterDescriptor.data())});
        warmUp.insert(warmUp.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
        warmUp.insert(warmUp.end(), {0xc0033700u, 0x00100200u, Low(&Done), High(&Done), 2u});
        Packet first{warmUp.data(), static_cast<std::uint32_t>(warmUp.size()), 0, {}};
        Require(sceAgcDriverSubmitAcb(0x20, &first) == 0, "warm-up submit failed");
        AgcDriverWaitIdle_nid_postfix();
        Require(Done == 2 && Counter[0] == 1, "the warm-up dispatch did not run");
        Counter[0] = 0;
        std::vector<std::uint32_t> words;
        words.insert(words.end(), {0xc0053c00u, 0x13u, Low(&Gate), High(&Gate), 1u, 0xffffffffu, 0x19u});
        Program(words, WriterCode.data(), {Low(ArgumentsDescriptor.data()), High(ArgumentsDescriptor.data()), 2u, 3u, 4u});
        words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
        Program(words, CounterCode.data(), {Low(CounterDescriptor.data()), High(CounterDescriptor.data())});
        words.insert(words.end(), {0xc0021102u, 1u, Low(Arguments.data()), High(Arguments.data())});
        words.insert(words.end(), {0xc0011600u, 0u, 0x8061u});
        words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(&Done), High(&Done), 1u});
        Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
        Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "compute submit failed");
        AgcDriverWaitIdle_nid_postfix();
        Require(Done == 1, "the submission did not finish");
        Require(Arguments[0] == 2 && Arguments[1] == 3 && Arguments[2] == 4, "the first dispatch did not write the indirect arguments");
        Require(Counter[0] == 24, "an indirect dispatch whose arguments the dispatch before it writes ran " + std::to_string(Counter[0]) + " groups, expected 24");
        std::cout << "indirect arguments written ahead test passed\n";
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
