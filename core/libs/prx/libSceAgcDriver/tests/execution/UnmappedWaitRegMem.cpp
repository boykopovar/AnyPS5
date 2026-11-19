#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#ifndef _WIN32
#include <sys/mman.h>
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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

alignas(256) std::array<std::uint32_t, 15> SpinCode{0xf4080200u, 0xfa000000u, 0xbe840381u, 0x9304ff04u, 0x0019660du, 0x8004ff04u, 0x3c6ef35fu, 0x80828102u, 0xbf078002u, 0xbf85fff9u, 0xbf8cc07fu, 0x7e000204u, 0xe0700000u, 0x80020000u, 0xbf810000u};
constexpr std::uint32_t SpinIterations = 1u << 20u;

alignas(256) std::array<std::uint32_t, 4> SpinOutput{};
alignas(256) std::array<std::uint32_t, 4> SpinDescriptor{};
alignas(256) volatile std::uint32_t Label = 0;

std::uint32_t Low(const volatile void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); }
std::uint32_t High(const volatile void* pointer) { return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u); }

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

void ReleaseLabel(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), Low(target), High(target), value, 0u, 0u});
}

void WaitEqual32(std::vector<std::uint32_t>& words, const volatile void* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0053c00u, 0x13u, Low(target), High(target), value, 0xffffffffu, 0x19u});
}

void WaitEqual64(std::vector<std::uint32_t>& words, const volatile void* target, std::uint64_t value) {
    words.insert(words.end(), {0xc0079300u, 0x13u, Low(target), High(target), static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32u), 0xffffffffu, 0xffffffffu, 0x19u});
}

class UnmappedPage {
public:
    UnmappedPage() {
#ifdef _WIN32
        memory = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, Bytes, MEM_RESERVE, PAGE_NOACCESS));
#else
        void* raw = mmap(nullptr, Bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        memory = raw == MAP_FAILED ? nullptr : static_cast<std::uint8_t*>(raw);
#endif
        Require(memory != nullptr, "unmapped page: cannot reserve the address range");
    }

    ~UnmappedPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, Bytes);
#endif
    }

    UnmappedPage(const UnmappedPage&) = delete;
    UnmappedPage& operator=(const UnmappedPage&) = delete;

    const volatile void* Address() const { return memory; }

private:
    static constexpr std::size_t Bytes = 65536;
    std::uint8_t* memory = nullptr;
};

void RunWait(bool wide, const volatile void* unmapped) {
    SpinDescriptor = {Low(SpinOutput.data()), High(SpinOutput.data()) & 0xffffu, sizeof(SpinOutput), 0x31016facu};
    std::vector<std::uint32_t> words;
    Dispatch(words, SpinCode.data(), {Low(SpinDescriptor.data()), High(SpinDescriptor.data()), SpinIterations});
    ReleaseLabel(words, &Label, 1);
    if (wide) WaitEqual64(words, unmapped, 1);
    else WaitEqual32(words, unmapped, 1);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "graphics submit failed");
    AgcDriverWaitIdle_nid_postfix();
}

}

int main(int argc, char** argv) {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        const bool wide = argc == 2 && std::string(argv[1]) == "--wait64";
        UnmappedPage page;
        bool refused = false;
        try {
            RunWait(wide, page.Address());
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            refused = message.find("guest memory is not readable") != std::string::npos;
            Require(refused, std::string(wide ? "WAIT_REG_MEM_64" : "WAIT_REG_MEM") + " on an unmapped address failed with the wrong error: " + message);
        }
        Require(refused, std::string(wide ? "WAIT_REG_MEM_64" : "WAIT_REG_MEM") + " on an unmapped address while the draw pipeline was busy was not refused");
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
        std::cout << (wide ? "WAIT_REG_MEM_64" : "WAIT_REG_MEM") << " on an unmapped address was refused with a guest memory error\n";
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
