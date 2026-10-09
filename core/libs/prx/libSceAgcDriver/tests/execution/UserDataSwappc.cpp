#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) std::array<std::uint32_t, 16> Output;
alignas(256) std::array<std::uint32_t, 2> First{0x80148714u, 0xbe80200eu};
alignas(256) std::array<std::uint32_t, 2> Second{0x80148b14u, 0xbe80200eu};

void CaptureChecks(AgcDriver::VulkanDevice& device) {
    using AgcDriver::DriverDetail::InvocationFor;
    using AgcDriver::DriverDetail::ShaderSnapshot;
    ShaderSnapshot snapshot{0x10000u, 0, 0, {0xbe940382u, 0xbe8e0304u, 0xbe8f0305u, 0xbe8e210eu, 0xbf810000u}, {}};
    std::array<std::uint32_t, 6> users{};
    const auto address = reinterpret_cast<std::uintptr_t>(First.data());
    users[4] = static_cast<std::uint32_t>(address);
    users[5] = static_cast<std::uint32_t>(address >> 32u);
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Compute, snapshot.codeAddress, snapshot.code, 0, {}};
    request.context.userData = users;
    request.context.waveSize = 32;
    request.context.compute = ShaderRecompiler::ShaderComputeStageInfo{{1, 1, 1}, 0, {false, false, false}, false, 0};
    request.target = device.ComputeTarget(32);
    request.layout = {0, 0, 0, 128};
    AgcDriver::ShaderMemory memory({});
    const auto invocation = InvocationFor(snapshot, 0, request, &memory);
    const auto capture = memory.Capture(invocation);
    const auto handle = AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, invocation.Request());
    auto stale = invocation.Request();
    const auto otherAddress = reinterpret_cast<std::uintptr_t>(Second.data());
    users[4] = static_cast<std::uint32_t>(otherAddress);
    users[5] = static_cast<std::uint32_t>(otherAddress >> 32u);
    Require(!ShaderRecompiler::MatchesPreparedShader(stale, *handle), "prepared artifact accepts another invocation's function pointer");
    bool staleRejected = false;
    try {
        static_cast<void>(ShaderRecompiler::PrepareShader(stale));
    } catch (const std::exception& error) {
        staleRejected = std::string(error.what()).find("do not match invocation user data") != std::string::npos;
    }
    Require(staleRejected, "source cache accepts stale call bindings");
    users[4] = static_cast<std::uint32_t>(address);
    users[5] = static_cast<std::uint32_t>(address >> 32u);
    Require(std::ranges::find(capture->readTrace.otherReads, address) != capture->readTrace.otherReads.end(), "callee code may be masked as flat resource data");
    Require(std::ranges::find(capture->readTrace.otherReads, address + 4u) != capture->readTrace.otherReads.end(), "callee return bytes were not captured");
    const auto regions = memory.Regions();
    Require(std::ranges::any_of(regions, [&](const auto& region) { return region.guestAddress <= address && address - region.guestAddress + sizeof(First) <= region.bytes.size(); }), "dispatch cache does not observe callee code");
    const std::array<ShaderRecompiler::MemoryRegion, 1> frozenCode{{{address, std::as_bytes(std::span(First))}}};
    AgcDriver::ShaderMemory frozen(frozenCode);
    bool aliasRejected = false;
    try {
        static_cast<void>(InvocationFor(snapshot, 0, request, &frozen));
    } catch (const std::exception& error) {
        aliasRejected = std::string(error.what()).find("aliases a registered shader snapshot") != std::string::npos;
    }
    Require(aliasRejected, "callee bytes were read from an immutable registration snapshot");
    const auto reject = [&](std::uint64_t target, const char* expected) {
        users[4] = static_cast<std::uint32_t>(target);
        users[5] = static_cast<std::uint32_t>(target >> 32u);
        std::string previous;
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            AgcDriver::ShaderMemory invalid({});
            bool rejected = false;
            try {
                static_cast<void>(InvocationFor(snapshot, 0, request, &invalid));
            } catch (const std::exception& error) {
                const std::string reason = error.what();
                Require(reason.find(expected) != std::string::npos, "invalid target raised the wrong error: " + reason);
                Require(previous.empty() || previous == reason, "invalid target did not fail deterministically");
                previous = reason;
                rejected = true;
            }
            Require(rejected, "invalid target was accepted");
        }
        Require(!previous.empty(), "invalid target was accepted");
    };
    reject(0, "null or misaligned");
    reject(address + 1u, "null or misaligned");
    reject(0x1000000000000000ull, "guest memory");
}

void Append(std::vector<std::uint32_t>& code, std::uint32_t source, std::uint32_t output) {
    code.insert(code.end(), {0xbe920300u | source, 0xbe930300u | (source + 1u), 0xbe8e0312u, 0xbe8f0313u, 0xbe8e210eu,
        0x7e020214u, 0xe0700000u | output * 8u, 0x80000100u, 0x80148314u, 0x7e020214u, 0xe0700004u | output * 8u, 0x80000100u});
}

void Run(std::span<const std::uint32_t> sources, bool loop, std::uint32_t wave) {
    alignas(256) std::array<std::uint32_t, 128> code{};
    std::vector<std::uint32_t> instructions{0xbe940382u, 0x7e000280u, 0xbe960382u};
    const auto loopStart = instructions.size();
    for (std::size_t index = 0; index < sources.size(); ++index) Append(instructions, sources[index], static_cast<std::uint32_t>(index));
    if (loop) {
        instructions.push_back(0x80968116u);
        instructions.push_back(0xbf068016u);
        const auto displacement = static_cast<std::int32_t>(loopStart) - static_cast<std::int32_t>(instructions.size()) - 1;
        instructions.push_back(0xbf840000u | (static_cast<std::uint32_t>(displacement) & 0xffffu));
    }
    instructions.push_back(0xbf810000u);
    Require(instructions.size() <= code.size(), "synthetic caller exceeds its storage");
    std::copy(instructions.begin(), instructions.end(), code.begin());
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
        ShaderSpecialRegs specials{};
    } header;
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.shader.specials = &header.specials;
    header.specials.dispatch_modifier = wave == 32u ? 0x8000u : 0u;
    header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)},
        {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 24}}};
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    AgcDriverResolveShaderAbi_nid_postfix(&header.shader, {}, {});

    std::array<std::uint32_t, 12> users{};
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    users[0] = static_cast<std::uint32_t>(outputAddress);
    users[1] = static_cast<std::uint32_t>(outputAddress >> 32u) & 0xffffu;
    users[2] = sizeof(Output);
    users[3] = 0x31016facu;
    const auto bind = [&](std::size_t index, const void* pointer) {
        const auto value = reinterpret_cast<std::uintptr_t>(pointer);
        users[index] = static_cast<std::uint32_t>(value);
        users[index + 1u] = static_cast<std::uint32_t>(value >> 32u);
    };
    bind(4, First.data());
    bind(8, Second.data());
    const auto dispatch = [&](std::uint32_t firstAdd, std::uint32_t secondAdd) {
        Output.fill(0xdeadbeefu);
        std::vector<std::uint32_t> commands;
        for (const auto reg : header.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
        for (std::size_t index = 0; index < users.size(); ++index) commands.insert(commands.end(), {0xc0017600u, 0x240u + static_cast<std::uint32_t>(index), users[index]});
        commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x41u | (wave == 32u ? 0x8000u : 0u)});
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        sceAgcDriverSubmitAcb(0x20, &packet);
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(outputAddress, sizeof(Output));
        std::uint32_t expected = 2;
        for (std::uint32_t iteration = 0; iteration < (loop ? 2u : 1u); ++iteration) {
            for (std::size_t index = 0; index < sources.size(); ++index) {
                expected += sources[index] == 4u ? firstAdd : secondAdd;
                if (!loop || iteration == 1u) Require(Output[index * 2u] == expected, "callee result at site " + std::to_string(index) + " is " + std::to_string(Output[index * 2u]) + ", expected " + std::to_string(expected) + ", wave " + std::to_string(wave) + ", loop " + std::to_string(loop) + ", first add " + std::to_string(firstAdd) + ", second add " + std::to_string(secondAdd));
                expected += 3;
                if (!loop || iteration == 1u) Require(Output[index * 2u + 1u] == expected, "caller result at site " + std::to_string(index) + " is " + std::to_string(Output[index * 2u + 1u]) + ", expected " + std::to_string(expected));
            }
        }
    };
    dispatch(7, 11);
    dispatch(7, 11);
    bind(4, Second.data());
    dispatch(11, 11);
    bind(4, First.data());
    First[0] = 0x80148d14u;
    dispatch(13, 11);
    First[0] = 0x80148714u;
    dispatch(7, 11);
    Second[0] = 0x80149114u;
    dispatch(7, 17);
    Second[0] = 0x80148b14u;
    dispatch(7, 11);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CaptureChecks(*device);
        const std::array<std::uint32_t, 1> single{4};
        const std::array<std::uint32_t, 4> multiple{4, 8, 8, 8};
        Run(single, false, 32);
        Run(multiple, false, 32);
        Run(multiple, true, 32);
        Run(multiple, false, 64);
        AgcDriverShutdown_nid_postfix();
        std::puts("user-data swappc dispatch and cache tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception&) {}
        return 1;
    }
}
