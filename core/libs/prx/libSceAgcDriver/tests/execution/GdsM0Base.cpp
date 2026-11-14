#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Base = 0x100;
constexpr std::uint32_t Size = 0x100;
constexpr std::uint32_t Words = 4;
constexpr std::array<std::uint32_t, Words> Starts{1000, 2000, 3000, 4000};
constexpr std::array<std::uint32_t, Words> Canaries{7, 8, 9, 10};
constexpr std::uint32_t Untouched = 0x5eed0004u;

alignas(256) std::array<std::uint32_t, MaxThreads> Output{};
alignas(16) std::array<std::uint32_t, Words> Readback{};

alignas(256) auto AddCode = std::to_array<std::uint32_t>({
    0x7e020280,
    0x7e040281,
    0xbefc03ff, (Base << 16u) | Size,
    0xd8820004, 0x03000201,
    0xbf8cc07f,
    0xe0702000, 0x80000300,
    0xbf810000,
});

alignas(256) auto ReadCode = std::to_array<std::uint32_t>({
    0x7e020280,
    0x7e0602ff, Untouched,
    0xbefc03ff, (Base << 16u) | Size,
    0xd8da0008, 0x03000001,
    0xbf8cc07f,
    0xe0702000, 0x80000300,
    0xbf810000,
});

struct Workgroup {
    std::uint32_t threads;
    std::uint32_t waveSize;

    std::string Name() const {
        return "wave" + std::to_string(waveSize) + " " + std::to_string(threads) + " threads";
    }
};

constexpr std::array<Workgroup, 3> Workgroups{{{32, 32}, {64, 32}, {64, 64}}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

std::vector<std::uint32_t> Packet(std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    std::vector<std::uint32_t> result{0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u)};
    result.insert(result.end(), payload);
    return result;
}

void Execute(AgcDriver::QueueState& queue, const std::vector<std::uint32_t>& packet) {
    AgcDriver::Pm4::Validate(packet, 0);
    AgcDriver::Pm4::Execute(packet, queue);
}

void SetWords(AgcDriver::QueueState& queue, std::uint32_t offset, const std::array<std::uint32_t, Words>& values) {
    for (std::uint32_t word = 0; word < Words; ++word) Execute(queue, Packet(0x50, {0x40100000u, values[word], 0, offset + word * 4u, 0, 4}));
}

std::array<std::uint32_t, Words> ReadWords(AgcDriver::QueueState& queue, std::uint32_t offset) {
    Readback.fill(0xdeadbeefu);
    const auto address = reinterpret_cast<std::uintptr_t>(Readback.data());
    Execute(queue, Packet(0x50, {0x20000000u, offset, 0, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), Words * 4u}));
    return Readback;
}

void Run(AgcDriver::VulkanDevice& device, const Workgroup& workgroup, std::span<const std::uint32_t> code) {
    Output.fill(Untouched);
    std::vector<std::uint32_t> userData(4, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{workgroup.threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {workgroup.waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::string Words4(const std::array<std::uint32_t, Words>& values) {
    std::string text;
    for (const auto value : values) text += (text.empty() ? "" : " ") + std::to_string(value);
    return text;
}

void CheckAdd(AgcDriver::QueueState& queue, const Workgroup& workgroup) {
    const auto what = workgroup.Name() + " ds_add_rtn_u32 gds with M0 base " + std::to_string(Base);
    std::vector<std::uint32_t> old(Output.begin(), Output.begin() + workgroup.threads);
    std::sort(old.begin(), old.end());
    for (std::uint32_t lane = 0; lane < workgroup.threads; ++lane) {
        Require(old[lane] == Starts[1] + lane, what + ": the returned values are not each of " + std::to_string(Starts[1]) + " to " + std::to_string(Starts[1] + workgroup.threads - 1) + " once (sorted value " + std::to_string(lane) + " is " + std::to_string(old[lane]) + ")");
    }
    auto expected = Starts;
    expected[1] += workgroup.threads;
    const auto atBase = ReadWords(queue, Base);
    Require(atBase == expected, what + ": GDS at M0 base + offset holds " + Words4(atBase) + ", expected " + Words4(expected));
    const auto atZero = ReadWords(queue, 0);
    Require(atZero == Canaries, what + ": GDS at address 0 + offset holds " + Words4(atZero) + ", expected the untouched " + Words4(Canaries));
}

void CheckRead(const Workgroup& workgroup) {
    const auto what = workgroup.Name() + " ds_read_b32 gds with M0 base " + std::to_string(Base);
    for (std::uint32_t lane = 0; lane < workgroup.threads; ++lane) {
        Require(Output[lane] == Starts[2], what + ": lane " + std::to_string(lane) + " read " + std::to_string(Output[lane]) + ", expected " + std::to_string(Starts[2]) + " from M0 base + offset 8");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32) {
            std::printf("skipped, the device's subgroups are narrower than a wave32 (%u lanes)\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        AgcDriver::QueueState queue;
        for (const auto& workgroup : Workgroups) {
            SetWords(queue, Base, Starts);
            SetWords(queue, 0, Canaries);
            Require(ReadWords(queue, Base) == Starts && ReadWords(queue, 0) == Canaries, workgroup.Name() + ": the GDS words did not read back as set");
            Run(*device, workgroup, AddCode);
            CheckAdd(queue, workgroup);
            SetWords(queue, Base, Starts);
            SetWords(queue, 0, Canaries);
            Run(*device, workgroup, ReadCode);
            CheckRead(workgroup);
        }
        std::puts("gds m0 base tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
