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

constexpr std::uint32_t ValueCount = 16;
constexpr std::uint32_t Threads = ValueCount * ValueCount;
constexpr std::uint32_t Groups = Threads / 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

constexpr std::uint32_t Values[ValueCount] = {
    0x0000u, 0x8000u, 0x0001u, 0x8001u, 0x03ffu, 0x83ffu, 0x0400u, 0x8400u, 0x3c00u, 0xbc00u, 0x7c00u, 0xfc00u, 0x7e00u, 0x7c01u, 0xfe00u, 0x7bffu,
};

alignas(256) constexpr std::array<std::uint32_t, 48> CompareCode{
    0x34020084u, 0x34040086u, 0xe0341000u, 0x80000401u, 0xbf8c3f70u, 0x7d940b04u, 0xd501000au, 0x01a90280u,
    0x7d9c0b04u, 0xd501000bu, 0x01a90280u, 0x7d980b04u, 0xd501000cu, 0x01a90280u, 0x7d960b04u, 0xd501000du,
    0x01a90280u, 0x7d9a0b04u, 0xd501000eu, 0x01a90280u, 0x7d920b04u, 0xd501000fu, 0x01a90280u, 0x7dda0b04u,
    0xd5010010u, 0x01a90280u, 0x7dd20b04u, 0xd5010011u, 0x01a90280u, 0x7dd60b04u, 0xd5010012u, 0x01a90280u,
    0x7dd80b04u, 0xd5010013u, 0x01a90280u, 0x7dd40b04u, 0xd5010014u, 0x01a90280u, 0x7ddc0b04u, 0xd5010015u,
    0x01a90280u, 0xe0781000u, 0x80010a02u, 0xe0781010u, 0x80010e02u, 0xe0781020u, 0x80011202u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 84> CompareExecCode{
    0x34020084u, 0x34040086u, 0xe0341000u, 0x80000401u, 0xbf8c3f70u, 0xbe94037eu, 0x7db40b04u, 0xbe95037eu,
    0xbefe0314u, 0xd501000au, 0x00550280u, 0xbe94037eu, 0x7dbc0b04u, 0xbe95037eu, 0xbefe0314u, 0xd501000bu,
    0x00550280u, 0xbe94037eu, 0x7db80b04u, 0xbe95037eu, 0xbefe0314u, 0xd501000cu, 0x00550280u, 0xbe94037eu,
    0x7db60b04u, 0xbe95037eu, 0xbefe0314u, 0xd501000du, 0x00550280u, 0xbe94037eu, 0x7dba0b04u, 0xbe95037eu,
    0xbefe0314u, 0xd501000eu, 0x00550280u, 0xbe94037eu, 0x7db20b04u, 0xbe95037eu, 0xbefe0314u, 0xd501000fu,
    0x00550280u, 0xbe94037eu, 0x7dfa0b04u, 0xbe95037eu, 0xbefe0314u, 0xd5010010u, 0x00550280u, 0xbe94037eu,
    0x7df20b04u, 0xbe95037eu, 0xbefe0314u, 0xd5010011u, 0x00550280u, 0xbe94037eu, 0x7df60b04u, 0xbe95037eu,
    0xbefe0314u, 0xd5010012u, 0x00550280u, 0xbe94037eu, 0x7df80b04u, 0xbe95037eu, 0xbefe0314u, 0xd5010013u,
    0x00550280u, 0xbe94037eu, 0x7df40b04u, 0xbe95037eu, 0xbefe0314u, 0xd5010014u, 0x00550280u, 0xbe94037eu,
    0x7dfc0b04u, 0xbe95037eu, 0xbefe0314u, 0xd5010015u, 0x00550280u, 0xe0781000u, 0x80010a02u, 0xe0781010u,
    0x80010e02u, 0xe0781020u, 0x80011202u, 0xbf810000u,
};

struct Row {
    const char* condition;
    std::uint32_t inputsPreserved[Groups];
    std::uint32_t inputsFlushed[Groups];
};

constexpr Row Rows[] = {
    {"eq",
     {0x00030003u, 0x00080004u, 0x00200010u, 0x00800040u, 0x02000100u, 0x08000400u, 0x00000000u, 0x80000000u},
     {0x003f003fu, 0x003f003fu, 0x003f003fu, 0x00800040u, 0x02000100u, 0x08000400u, 0x00000000u, 0x80000000u}},
    {"ge",
     {0x0aab0aabu, 0x0aa80aafu, 0x0aa00abfu, 0x0a800affu, 0x0a000bffu, 0x08008fffu, 0x00000000u, 0x8bff0000u},
     {0x0abf0abfu, 0x0abf0abfu, 0x0abf0abfu, 0x0a800affu, 0x0a000bffu, 0x08008fffu, 0x00000000u, 0x8bff0000u}},
    {"gt",
     {0x0aa80aa8u, 0x0aa00aabu, 0x0a800aafu, 0x0a000abfu, 0x08000affu, 0x00008bffu, 0x00000000u, 0x0bff0000u},
     {0x0a800a80u, 0x0a800a80u, 0x0a800a80u, 0x0a000abfu, 0x08000affu, 0x00008bffu, 0x00000000u, 0x0bff0000u}},
    {"le",
     {0x85578557u, 0x855f8554u, 0x857f8550u, 0x85ff8540u, 0x87ff8500u, 0x8fff0400u, 0x00000000u, 0x84000000u},
     {0x857f857fu, 0x857f857fu, 0x857f857fu, 0x85ff8540u, 0x87ff8500u, 0x8fff0400u, 0x00000000u, 0x84000000u}},
    {"lg",
     {0x8ffc8ffcu, 0x8ff78ffbu, 0x8fdf8fefu, 0x8f7f8fbfu, 0x8dff8effu, 0x87ff8bffu, 0x00000000u, 0x0fff0000u},
     {0x8fc08fc0u, 0x8fc08fc0u, 0x8fc08fc0u, 0x8f7f8fbfu, 0x8dff8effu, 0x87ff8bffu, 0x00000000u, 0x0fff0000u}},
    {"lt",
     {0x85548554u, 0x85578550u, 0x855f8540u, 0x857f8500u, 0x85ff8400u, 0x87ff0000u, 0x00000000u, 0x04000000u},
     {0x85408540u, 0x85408540u, 0x85408540u, 0x857f8500u, 0x85ff8400u, 0x87ff0000u, 0x00000000u, 0x04000000u}},
    {"neq",
     {0xfffcfffcu, 0xfff7fffbu, 0xffdfffefu, 0xff7fffbfu, 0xfdfffeffu, 0xf7fffbffu, 0xffffffffu, 0x7fffffffu},
     {0xffc0ffc0u, 0xffc0ffc0u, 0xffc0ffc0u, 0xff7fffbfu, 0xfdfffeffu, 0xf7fffbffu, 0xffffffffu, 0x7fffffffu}},
    {"nge",
     {0xf554f554u, 0xf557f550u, 0xf55ff540u, 0xf57ff500u, 0xf5fff400u, 0xf7ff7000u, 0xffffffffu, 0x7400ffffu},
     {0xf540f540u, 0xf540f540u, 0xf540f540u, 0xf57ff500u, 0xf5fff400u, 0xf7ff7000u, 0xffffffffu, 0x7400ffffu}},
    {"ngt",
     {0xf557f557u, 0xf55ff554u, 0xf57ff550u, 0xf5fff540u, 0xf7fff500u, 0xffff7400u, 0xffffffffu, 0xf400ffffu},
     {0xf57ff57fu, 0xf57ff57fu, 0xf57ff57fu, 0xf5fff540u, 0xf7fff500u, 0xffff7400u, 0xffffffffu, 0xf400ffffu}},
    {"nle",
     {0x7aa87aa8u, 0x7aa07aabu, 0x7a807aafu, 0x7a007abfu, 0x78007affu, 0x7000fbffu, 0xffffffffu, 0x7bffffffu},
     {0x7a807a80u, 0x7a807a80u, 0x7a807a80u, 0x7a007abfu, 0x78007affu, 0x7000fbffu, 0xffffffffu, 0x7bffffffu}},
    {"nlg",
     {0x70037003u, 0x70087004u, 0x70207010u, 0x70807040u, 0x72007100u, 0x78007400u, 0xffffffffu, 0xf000ffffu},
     {0x703f703fu, 0x703f703fu, 0x703f703fu, 0x70807040u, 0x72007100u, 0x78007400u, 0xffffffffu, 0xf000ffffu}},
    {"nlt",
     {0x7aab7aabu, 0x7aa87aafu, 0x7aa07abfu, 0x7a807affu, 0x7a007bffu, 0x7800ffffu, 0xffffffffu, 0xfbffffffu},
     {0x7abf7abfu, 0x7abf7abfu, 0x7abf7abfu, 0x7a807affu, 0x7a007bffu, 0x7800ffffu, 0xffffffffu, 0xfbffffffu}},
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%04x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, ShaderRecompiler::ShaderFloatMode mode) {
    for (std::uint32_t pair = 0; pair < Threads; ++pair) {
        Input[pair * Inputs] = Values[pair / ValueCount];
        Input[pair * Inputs + 1u] = Values[pair % ValueCount];
        Input[pair * Inputs + 2u] = 0u;
        Input[pair * Inputs + 3u] = 0u;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
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
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const char* form, const char* modeName, bool flushed) {
    for (std::uint32_t index = 0; index < std::size(Rows); ++index) {
        const Row& row = Rows[index];
        for (std::uint32_t pair = 0; pair < Threads; ++pair) {
            const std::uint32_t actual = Output[pair * Results + index];
            const std::uint32_t expected = ((flushed ? row.inputsFlushed : row.inputsPreserved)[pair / 32u] >> (pair % 32u)) & 1u;
            Require(actual == expected, std::string(form) + row.condition + "_f16 " + modeName + " of " + Hex(Values[pair / ValueCount]) + " and " +
                                            Hex(Values[pair % ValueCount]) + " is " + Hex(actual) + ", expected " + std::to_string(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        struct Mode {
            const char* name;
            ShaderRecompiler::ShaderFloatMode mode;
            bool flushed;
        };
        const Mode modes[] = {
            {"f16 denormal mode 0, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x30u, true, true, false}, true},
            {"f16 denormal mode 0, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x30u, true, false, false}, true},
            {"f16 denormal mode 1, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x70u, true, true, false}, false},
            {"f16 denormal mode 1, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x70u, true, false, false}, false},
            {"f16 denormal mode 2, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xb0u, true, true, false}, true},
            {"f16 denormal mode 2, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xb0u, true, false, false}, true},
            {"f16 denormal mode 3, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, false},
            {"f16 denormal mode 3, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}, false},
        };
        for (const Mode& mode : modes) {
            Run(*device, CompareCode, mode.mode);
            Check("v_cmp_", mode.name, mode.flushed);
            Run(*device, CompareExecCode, mode.mode);
            Check("v_cmpx_", mode.name, mode.flushed);
        }
        std::puts("v_cmp and v_cmpx f16 denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
