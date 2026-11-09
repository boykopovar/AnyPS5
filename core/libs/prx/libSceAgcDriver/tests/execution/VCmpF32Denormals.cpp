#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t ValueCount = 16;
constexpr std::uint32_t Groups = ValueCount * ValueCount / Threads;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

constexpr std::uint32_t Values[ValueCount] = {
    0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u, 0x007fffffu, 0x807fffffu, 0x00800000u, 0x80800000u,
    0x3f800000u, 0xbf800000u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7f800001u, 0xffc00000u, 0x7f7fffffu,
};

constexpr std::uint32_t CaptureExec = 0xbe95037eu;
constexpr std::uint32_t CaptureVcc = 0xbe95036au;

alignas(256) std::array<std::uint32_t, 14> Code{
    0x34020084u, 0x34060082u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xbe94037eu, 0x7c240b04u,
    0xbe95037eu, 0xbefe0314u, 0xd501000au, 0x00550280u, 0xe0701000u, 0x80010a03u, 0xbf810000u,
};

struct Row {
    const char* condition;
    std::uint32_t cmpx;
    std::uint32_t cmp;
    std::uint32_t kept[Groups];
    std::uint32_t flushed[Groups];
};

constexpr std::uint32_t RowCount = 12;
constexpr Row Rows[RowCount] = {
    {"eq", 0x7c240b04u, 0x7c040b04u,
     {0x00030003u, 0x00080004u, 0x00200010u, 0x00800040u, 0x02000100u, 0x08000400u, 0x00000000u, 0x80000000u},
     {0x003f003fu, 0x003f003fu, 0x003f003fu, 0x00800040u, 0x02000100u, 0x08000400u, 0x00000000u, 0x80000000u}},
    {"ge", 0x7c2c0b04u, 0x7c0c0b04u,
     {0x0aab0aabu, 0x0aa80aafu, 0x0aa00abfu, 0x0a800affu, 0x0a000bffu, 0x08008fffu, 0x00000000u, 0x8bff0000u},
     {0x0abf0abfu, 0x0abf0abfu, 0x0abf0abfu, 0x0a800affu, 0x0a000bffu, 0x08008fffu, 0x00000000u, 0x8bff0000u}},
    {"gt", 0x7c280b04u, 0x7c080b04u,
     {0x0aa80aa8u, 0x0aa00aabu, 0x0a800aafu, 0x0a000abfu, 0x08000affu, 0x00008bffu, 0x00000000u, 0x0bff0000u},
     {0x0a800a80u, 0x0a800a80u, 0x0a800a80u, 0x0a000abfu, 0x08000affu, 0x00008bffu, 0x00000000u, 0x0bff0000u}},
    {"le", 0x7c260b04u, 0x7c060b04u,
     {0x85578557u, 0x855f8554u, 0x857f8550u, 0x85ff8540u, 0x87ff8500u, 0x8fff0400u, 0x00000000u, 0x84000000u},
     {0x857f857fu, 0x857f857fu, 0x857f857fu, 0x85ff8540u, 0x87ff8500u, 0x8fff0400u, 0x00000000u, 0x84000000u}},
    {"lg", 0x7c2a0b04u, 0x7c0a0b04u,
     {0x8ffc8ffcu, 0x8ff78ffbu, 0x8fdf8fefu, 0x8f7f8fbfu, 0x8dff8effu, 0x87ff8bffu, 0x00000000u, 0x0fff0000u},
     {0x8fc08fc0u, 0x8fc08fc0u, 0x8fc08fc0u, 0x8f7f8fbfu, 0x8dff8effu, 0x87ff8bffu, 0x00000000u, 0x0fff0000u}},
    {"lt", 0x7c220b04u, 0x7c020b04u,
     {0x85548554u, 0x85578550u, 0x855f8540u, 0x857f8500u, 0x85ff8400u, 0x87ff0000u, 0x00000000u, 0x04000000u},
     {0x85408540u, 0x85408540u, 0x85408540u, 0x857f8500u, 0x85ff8400u, 0x87ff0000u, 0x00000000u, 0x04000000u}},
    {"neq", 0x7c3a0b04u, 0x7c1a0b04u,
     {0xfffcfffcu, 0xfff7fffbu, 0xffdfffefu, 0xff7fffbfu, 0xfdfffeffu, 0xf7fffbffu, 0xffffffffu, 0x7fffffffu},
     {0xffc0ffc0u, 0xffc0ffc0u, 0xffc0ffc0u, 0xff7fffbfu, 0xfdfffeffu, 0xf7fffbffu, 0xffffffffu, 0x7fffffffu}},
    {"nge", 0x7c320b04u, 0x7c120b04u,
     {0xf554f554u, 0xf557f550u, 0xf55ff540u, 0xf57ff500u, 0xf5fff400u, 0xf7ff7000u, 0xffffffffu, 0x7400ffffu},
     {0xf540f540u, 0xf540f540u, 0xf540f540u, 0xf57ff500u, 0xf5fff400u, 0xf7ff7000u, 0xffffffffu, 0x7400ffffu}},
    {"ngt", 0x7c360b04u, 0x7c160b04u,
     {0xf557f557u, 0xf55ff554u, 0xf57ff550u, 0xf5fff540u, 0xf7fff500u, 0xffff7400u, 0xffffffffu, 0xf400ffffu},
     {0xf57ff57fu, 0xf57ff57fu, 0xf57ff57fu, 0xf5fff540u, 0xf7fff500u, 0xffff7400u, 0xffffffffu, 0xf400ffffu}},
    {"nle", 0x7c380b04u, 0x7c180b04u,
     {0x7aa87aa8u, 0x7aa07aabu, 0x7a807aafu, 0x7a007abfu, 0x78007affu, 0x7000fbffu, 0xffffffffu, 0x7bffffffu},
     {0x7a807a80u, 0x7a807a80u, 0x7a807a80u, 0x7a007abfu, 0x78007affu, 0x7000fbffu, 0xffffffffu, 0x7bffffffu}},
    {"nlg", 0x7c340b04u, 0x7c140b04u,
     {0x70037003u, 0x70087004u, 0x70207010u, 0x70807040u, 0x72007100u, 0x78007400u, 0xffffffffu, 0xf000ffffu},
     {0x703f703fu, 0x703f703fu, 0x703f703fu, 0x70807040u, 0x72007100u, 0x78007400u, 0xffffffffu, 0xf000ffffu}},
    {"nlt", 0x7c3c0b04u, 0x7c1c0b04u,
     {0x7aab7aabu, 0x7aa87aafu, 0x7aa07abfu, 0x7a807affu, 0x7a007bffu, 0x7800ffffu, 0xffffffffu, 0xfbffffffu},
     {0x7abf7abfu, 0x7abf7abfu, 0x7abf7abfu, 0x7a807affu, 0x7a007bffu, 0x7800ffffu, 0xffffffffu, 0xfbffffffu}},
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

ShaderRecompiler::RecompileResult Compile(const AgcDriver::VulkanDevice& device, ShaderRecompiler::ShaderFloatMode mode, std::uint32_t compare, std::uint32_t capture) {
    Code[6] = compare;
    Code[7] = capture;
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = mode;
    return ShaderRecompiler::Recompile(request);
}

std::uint32_t Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& program, std::uint32_t group) {
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const std::uint32_t pair = group * Threads + lane;
        Input[lane * Inputs] = Values[pair / ValueCount];
        Input[lane * Inputs + 1u] = Values[pair % ValueCount];
        Input[lane * Inputs + 2u] = 0u;
        Input[lane * Inputs + 3u] = 0u;
    }
    Output.fill(0xdeadbeefu);
    device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    std::uint32_t mask = 0u;
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Require(Output[lane] <= 1u, "v_cmp f32 denormals: lane " + std::to_string(lane) + " stored " + Hex(Output[lane]));
        mask |= Output[lane] << lane;
    }
    return mask;
}

void Check(AgcDriver::VulkanDevice& device, const char* modeName, ShaderRecompiler::ShaderFloatMode mode, const Row& row, bool exec, const std::uint32_t* expected) {
    const std::string name = std::string(exec ? "v_cmpx_" : "v_cmp_") + row.condition + "_f32";
    const auto program = Compile(device, mode, exec ? row.cmpx : row.cmp, exec ? CaptureExec : CaptureVcc);
    for (std::uint32_t group = 0; group < Groups; ++group) {
        const std::uint32_t actual = Run(device, program, group);
        if (actual == expected[group]) continue;
        const std::uint32_t diff = actual ^ expected[group];
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            if ((diff >> lane & 1u) == 0u) continue;
            const std::uint32_t pair = group * Threads + lane;
            Require(false,
                    name + " " + modeName + " of " + Hex(Values[pair / ValueCount]) + " and " +
                        Hex(Values[pair % ValueCount]) + " is " + std::to_string(actual >> lane & 1u) + ", expected " +
                        std::to_string(expected[group] >> lane & 1u));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const Row& row : Rows) {
            for (const bool exec : {true, false}) {
                Check(*device, "denormals kept, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, row, exec, row.kept);
                Check(*device, "denormals kept, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xf0u, false, false, false}, row, exec, row.kept);
                Check(*device, "denormals flushed, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false}, row, exec, row.flushed);
                Check(*device, "denormals flushed, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xc0u, false, false, false}, row, exec, row.flushed);
            }
        }
        std::puts("v_cmp and v_cmpx f32 denormals tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
