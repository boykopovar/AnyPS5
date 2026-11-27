#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::RuntimeSwitch;

alignas(256) constexpr std::array<std::uint32_t, 11> LineCode{0x7e040300u, 0xd7460003u, 0x04091004u, 0x7e0a0502u, 0xd76f0004u, 0x04092005u, 0xd76f0005u, 0x04113004u, 0xe0702000u, 0x80000503u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 18> WindowCode{0x4a040081u, 0x4a060082u, 0x4a080083u, 0x4a0a0084u, 0xbefc0304u, 0x7e0c8702u, 0x7e048500u, 0x7e0e8702u, 0x7e128703u, 0xd76f0008u, 0x04191107u, 0xd76f0008u, 0x04212109u, 0xd746000au, 0x04010e04u, 0xe0702000u, 0x8000080au, 0xbf810000u};
constexpr std::uint32_t StoredFormat = 0x00014204u;
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::size_t OutputWords = 1024;
constexpr std::size_t BlockBytes = 65536;

struct Case {
    const char* name;
    std::span<const std::uint32_t> code;
    std::uint32_t threads;
    std::uint32_t wave;
};

std::uint64_t Fnv(std::span<const std::uint32_t> words) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const auto word : words) {
        for (std::uint32_t byte = 0; byte < 4u; ++byte) {
            hash ^= (word >> (8u * byte)) & 0xffu;
            hash *= 0x100000001b3ull;
        }
    }
    return hash;
}

std::uint64_t Compile(AgcDriver::VulkanDevice& device, const Case& item, bool window, bool* selects) {
    constexpr std::array<std::uint32_t, 4> userData{0x200000u, 4u << 16u, 1024u, StoredFormat};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{item.threads, 1, 1}, 0, {true, false, false}, false, 1, {}, 0};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(item.code.data()), item.code, 0, {}},
        {item.wave, 0, userData, compute, std::nullopt, std::nullopt, {}, std::nullopt},
        device.ComputeTarget(item.wave),
        {0, 0, 0, 128}};
    request.target.windowSelects = window;
    const auto result = ShaderRecompiler::Recompile(request);
    *selects = result.windowSelects;
    return Fnv(std::span<const std::uint32_t>(result.spirv.data(), result.spirv.size()));
}

void Forms(AgcDriver::VulkanDevice& device) {
    for (const Case& item : {Case{"line", LineCode, 256, 64}, Case{"line-wave32", LineCode, 256, 32}}) {
        bool selects = true;
        const auto plain = Compile(device, item, false, &selects);
        Require(!selects, std::string(item.name) + ": the plain request compiled the select form");
        const auto requested = Compile(device, item, true, &selects);
        Require(!selects && requested == plain, std::string(item.name) + ": a program without a register window compiled differently with window selects requested");
    }
    const Case window{"window", WindowCode, 128, 64};
    bool selects = true;
    const auto plain = Compile(device, window, false, &selects);
    Require(!selects, "the register-window program compiled the select form without the request");
    const auto selected = Compile(device, window, true, &selects);
    Require(selects && selected != plain, "the register-window program did not compile the select form when requested");
    std::printf("form window array %016llx selects %016llx\n", static_cast<unsigned long long>(plain), static_cast<unsigned long long>(selected));
}

std::span<std::uint32_t> GuestOutput() {
    std::uint8_t* block = nullptr;
#ifdef _WIN32
    block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
    if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
    constexpr std::uintptr_t alignment = 65536;
    void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped != MAP_FAILED) {
        const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
        const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
        if (aligned != begin) munmap(mapped, aligned - begin);
        if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
        block = reinterpret_cast<std::uint8_t*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
    }
#endif
    Require(block != nullptr, "cannot allocate the guest block for the output");
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return {reinterpret_cast<std::uint32_t*>(block), OutputWords};
}

struct Launch {
    std::span<const std::uint32_t> code;
    std::uint32_t threads;
    std::uint32_t groups;
};

std::vector<std::uint32_t> Run(std::span<std::uint32_t> output, const Launch& launch) {
    std::fill(output.begin(), output.end(), Untouched);
    const auto address = reinterpret_cast<std::uintptr_t>(output.data());
    const auto program = reinterpret_cast<std::uintptr_t>(launch.code.data());
    std::vector<std::uint32_t> words{
        0xc0037600u, 0x207, launch.threads, 1, 1,
        0xc0027600u, 0x20c, static_cast<std::uint32_t>(program >> 8u), static_cast<std::uint32_t>(program >> 40u),
        0xc0017600u, 0x213, (4u << 1u) | (1u << 7u),
        0xc0047600u, 0x240, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(output.size()), StoredFormat,
        0xc0031500u, launch.groups, 1, 1, 0x41u,
    };
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(address, output.size_bytes());
    return {output.begin(), output.end()};
}

std::vector<std::uint32_t> ExpectedLine() {
    std::vector<std::uint32_t> expected(OutputWords, Untouched);
    for (std::uint32_t thread = 0; thread < OutputWords; ++thread) {
        const auto group = thread / 256u;
        const auto linear = thread % 256u;
        expected.at(thread) = (group << 24u) | (linear / 64u * 64u << 16u) | linear;
    }
    return expected;
}

std::vector<std::uint32_t> ExpectedWindow() {
    std::vector<std::uint32_t> expected(OutputWords, Untouched);
    for (std::uint32_t group = 0; group < 4u; ++group) {
        for (std::uint32_t thread = 0; thread < 128u; ++thread) {
            std::array<std::uint32_t, 11> registers{};
            registers[0] = thread;
            for (std::uint32_t slot = 2; slot <= 5u; ++slot) registers[slot] = thread + slot - 1u;
            registers[6] = registers[2 + group];
            registers[2 + group] = thread;
            registers[7] = registers[2 + group];
            registers[9] = registers[3 + group];
            registers[8] = (registers[7] << 8u) | registers[6];
            registers[8] = (registers[9] << 16u) | registers[8];
            expected.at(group * 128u + thread) = registers[8];
        }
    }
    return expected;
}

void Same(const std::vector<std::uint32_t>& actual, const std::vector<std::uint32_t>& expected, const std::string& what) {
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (actual[index] != expected[index]) {
            char text[160];
            std::snprintf(text, sizeof(text), ": word %zu holds 0x%08x, expected 0x%08x", index, actual[index], expected[index]);
            throw std::runtime_error(what + text);
        }
    }
}

void Staged(std::span<std::uint32_t> output, const Launch& launch, const std::vector<std::uint32_t>& expected, bool selects, const std::string& name) {
    AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::WindowSelects, false);
    Same(Run(output, launch), expected, name + " with the switch unset");
    const auto before = AgcDriver::VulkanDevice::WindowSelectDispatches();
    AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::WindowSelects, true);
    Same(Run(output, launch), expected, name + " at the first dispatch with the switch set");
    Require(AgcDriver::VulkanDevice::WindowSelectDispatches() == before, name + ": the first dispatch with the switch set ran the select form before its pipeline was built in the background");
    AgcDriver::DriverDetail::AwaitLibraryPrebuilds();
    Same(Run(output, launch), expected, name + " after the background build with the switch set");
    Require(AgcDriver::VulkanDevice::WindowSelectDispatches() == before + (selects ? 1u : 0u), name + (selects ? ": the dispatch after the background build did not run the select form" : ": a program without a register window ran the select form"));
    AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::WindowSelects, false);
    Same(Run(output, launch), expected, name + " after the switch was unset again");
    Require(AgcDriver::VulkanDevice::WindowSelectDispatches() == before + (selects ? 1u : 0u), name + ": the select form ran after the switch was unset");
}

void Dispatches() {
    auto output = GuestOutput();
    Staged(output, {WindowCode, 128, 4}, ExpectedWindow(), true, "a register-window program");
    Staged(output, {LineCode, 256, 4}, ExpectedLine(), false, "a program without a register window");
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc == 2 ? argv[1] : "";
        Require(argc == 1 || mode == "--forms" || mode == "--dispatch", "usage: agc_driver_window_selects_tests [--forms|--dispatch]");
        {
            auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (argc == 1 || mode == "--forms") Forms(*device);
        }
        if (argc == 1 || mode == "--dispatch") Dispatches();
        AgcDriverShutdown_nid_postfix();
        std::puts("window select tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
