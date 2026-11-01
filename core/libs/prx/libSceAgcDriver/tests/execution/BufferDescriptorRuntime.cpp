#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t InputBytes = 256;
constexpr std::uint32_t Fill = 0xdeadbeefu;
alignas(256) std::array<std::uint8_t, InputBytes> Input{};
alignas(256) std::array<std::uint32_t, Lanes> Output{};

alignas(256) constexpr std::array<std::uint32_t, 7> Code{
    0x34020082, 0xe0301000, 0x80000201, 0xbf8c3f70, 0xe0701000, 0x80010201, 0xbf810000,
};

struct Case {
    const char* name;
    std::uint32_t baseOffset;
    std::uint32_t records;
    bool present;
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

ShaderRecompiler::RecompileRequest Request(std::span<const std::uint32_t> userData, const ShaderRecompiler::SpirvTarget& target, std::span<const ShaderRecompiler::MemoryRegion> memory) {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    return {
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
}

void Run(AgcDriver::VulkanDevice& device, bool registrationModules, bool coherent = false) {
    alignas(256) auto code = Code;
    if (coherent) code[1] |= 0x4000u;
    const std::string mode = registrationModules ? "registration modules" : "specialized modules";
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    std::array<std::uint32_t, 8> userData{};
    auto target = device.ComputeTarget(Lanes);
    target.registrationModules = registrationModules;
    auto request = Request(userData, target, memory);
    request.shader.code = code;
    request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
    const auto handle = ShaderRecompiler::PrepareShader(request);
    const std::array<Case, 7> cases{{
        {"aligned", 0, 128, true},
        {"base +1", 1, 128, true},
        {"base +2", 2, 128, true},
        {"base +3", 3, 128, true},
        {"records 37", 0, 37, true},
        {"records 64 base +2", 2, 64, true},
        {"absent", 0, 128, false},
    }};
    for (const auto& test : cases) {
        Output.fill(Fill);
        const auto input = test.present ? BufferDescriptor(Input.data() + test.baseOffset, test.records) : std::array<std::uint32_t, 4>{0u, 0u, 0u, 0x31016facu};
        const auto output = BufferDescriptor(Output.data(), sizeof(Output));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        ShaderRecompiler::SrtRuntime runtime{};
        runtime.userData = userData;
        const auto capture = ShaderRecompiler::CaptureResources(request, runtime, *handle);
        const auto result = ShaderRecompiler::MaterializeShader(request, *capture, *handle);
        Require(!result->registrationModule, "buffer descriptor runtime, " + mode + ", " + test.name + ": a compute shader was emitted in registration form");
        device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
        device.WaitIdle();
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            std::uint32_t expected = 0;
            if (test.present && lane * 4u + 4u <= test.records) std::memcpy(&expected, Input.data() + test.baseOffset + lane * 4u, sizeof(expected));
            Require(Output[lane] == expected, "buffer descriptor runtime, " + mode + ", " + test.name + ": lane " + std::to_string(lane) + " is " + Hex(Output[lane]) + ", expected " + Hex(expected));
        }
    }
}

void ScalarReads(AgcDriver::VulkanDevice& device) {
    constexpr std::array<std::uint32_t, 8> scalarCode{0x34020082u, 0xf4200200u, 0xfa000004u, 0xbf8cc07fu, 0x7e040208u, 0xe0701000u, 0x80010201u, 0xbf810000u};
    std::array<std::uint32_t, 8> userData{};
    const auto output = BufferDescriptor(Output.data(), sizeof(Output));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    auto request = Request(userData, device.ComputeTarget(Lanes), {});
    request.shader.code = scalarCode;
    request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(scalarCode.data());
    request.useCache = false;
    std::uint32_t expected = 0;
    std::memcpy(&expected, Input.data() + 4u, sizeof(expected));
    for (const auto ignoredWord1 : {0u, 0x40000000u}) {
        for (const auto word : {0u, 0x31016facu, 0x91b00f2eu, 0xffffffffu}) {
            auto input = BufferDescriptor(Input.data(), InputBytes);
            input[1] |= ignoredWord1;
            input[3] = word;
            std::copy(input.begin(), input.end(), userData.begin());
            Output.fill(Fill);
            const auto result = ShaderRecompiler::Recompile(request);
            device.Dispatch(result, 1, 1, 1);
            device.WaitIdle();
            Require(std::all_of(Output.begin(), Output.end(), [expected](std::uint32_t value) { return value == expected; }), "scalar buffer read changed with ignored descriptor word " + Hex(word));
        }
    }
    constexpr std::array<std::uint32_t, 13> mixedCode{0xf4200200u, 0xfa000004u, 0xbf8cc07fu, 0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0xe0701000u, 0x80010201u, 0x7e060208u, 0xe0701000u, 0x80010301u, 0xbf810000u};
    for (const auto code : {std::span<const std::uint32_t>(Code), std::span<const std::uint32_t>(mixedCode)}) {
        request.shader.code = code;
        request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
        try {
            static_cast<void>(ShaderRecompiler::Recompile(request));
        } catch (const std::runtime_error& error) {
            Require(std::string(error.what()).find("buffer descriptor uses an unsupported type") != std::string::npos, "vector buffer descriptor failed for another reason");
            char words[64];
            std::snprintf(words, sizeof(words), "(buffer 0 V# %08x %08x %08x %08x)", userData[0], userData[1], userData[2], userData[3]);
            Require(std::string(error.what()).find(words) != std::string::npos, std::string("unsupported buffer descriptor error does not name ") + words + ": " + error.what());
            continue;
        }
        throw std::runtime_error("a vector or mixed buffer access accepted a non-buffer descriptor type");
    }
    for (const auto code : {std::span<const std::uint32_t>(Code), std::span<const std::uint32_t>(mixedCode)}) {
        request.shader.code = code;
        request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
        auto input = BufferDescriptor(Input.data(), InputBytes);
        std::copy(input.begin(), input.end(), userData.begin());
        Output.fill(Fill);
        const auto baselineResult = ShaderRecompiler::Recompile(request);
        device.Dispatch(baselineResult, 1, 1, 1);
        device.WaitIdle();
        const auto baseline = Output;
        input[1] |= 0x40000000u;
        std::copy(input.begin(), input.end(), userData.begin());
        Output.fill(Fill);
        const auto result = ShaderRecompiler::Recompile(request);
        device.Dispatch(result, 1, 1, 1);
        device.WaitIdle();
        Require(Output == baseline, "vector or mixed buffer read changed with reserved bit 30 set");
    }
    request.shader.code = scalarCode;
    request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(scalarCode.data());
    struct BoundsCase {
        std::uint32_t stride;
        std::uint32_t records;
        bool inBounds;
    };
    constexpr std::array<BoundsCase, 8> bounds{{
        {0u, 0u, false}, {0u, 7u, false}, {0u, 8u, true}, {1u, 7u, false},
        {2u, 3u, false}, {2u, 4u, true}, {4u, 0x40000000u, true}, {0x3fffu, 0xffffffffu, true},
    }};
    GuestAllocations::Mutation().Add(Input.data(), Input.size(), true, false, true);
    for (const auto& test : bounds) {
        auto input = BufferDescriptor(Input.data(), test.records);
        input[1] |= test.stride << 16u;
        std::copy(input.begin(), input.end(), userData.begin());
        Output.fill(Fill);
        const auto result = ShaderRecompiler::Recompile(request);
        device.Dispatch(result, 1, 1, 1);
        device.WaitIdle();
        const auto value = test.inBounds ? expected : 0u;
        Require(std::all_of(Output.begin(), Output.end(), [value](std::uint32_t word) { return word == value; }), "scalar buffer bounds differ for stride " + std::to_string(test.stride) + " records " + std::to_string(test.records));
    }
    GuestAllocations::Mutation().Remove(Input.data());
}

void DisabledDescriptors(AgcDriver::VulkanDevice& device) {
    constexpr std::uint32_t Value = 0x12345678u;
    alignas(256) static constexpr std::array<std::uint32_t, 5> formattedStore{0x7e0202ffu, Value, 0xe0102000u, 0x80000100u, 0xbf810000u};
    alignas(256) static constexpr std::array<std::uint32_t, 6> rawStore{0x34020082u, 0x7e0402ffu, Value, 0xe0701000u, 0x80000201u, 0xbf810000u};
    alignas(256) static constexpr std::array<std::uint32_t, 7> formattedLoad{0x34020082u, 0xe0002000u, 0x80000200u, 0xbf8c3f70u, 0xe0701000u, 0x80010201u, 0xbf810000u};
    alignas(256) static constexpr std::array<std::uint32_t, 4> atomicAdd{0x34020082u, 0xe0c81000u, 0x80000201u, 0xbf810000u};
    std::array<std::uint32_t, 8> userData{};
    auto request = Request(userData, device.ComputeTarget(Lanes), {});
    request.useCache = false;
    const auto output = BufferDescriptor(Output.data(), sizeof(Output));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const auto dispatch = [&](std::span<const std::uint32_t> code, const std::array<std::uint32_t, 4>& descriptor) {
        request.shader.code = code;
        request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
        std::copy(descriptor.begin(), descriptor.end(), userData.begin());
        Output.fill(Fill);
        const auto result = ShaderRecompiler::Recompile(request);
        device.Dispatch(result, 1, 1, 1);
        device.WaitIdle();
    };
    const auto all = [](std::uint32_t expected) { return std::all_of(Output.begin(), Output.end(), [expected](std::uint32_t value) { return value == expected; }); };
    const auto raw = [](const void* data, std::uint32_t bytes, std::uint32_t word3) {
        auto descriptor = BufferDescriptor(data, bytes);
        descriptor[3] = word3;
        return descriptor;
    };
    const auto strided = [](const void* data, std::uint32_t word3) {
        auto descriptor = BufferDescriptor(data, Lanes);
        descriptor[1] |= 4u << 16u;
        descriptor[3] = word3;
        return descriptor;
    };
    std::array<std::uint32_t, Lanes> input{};
    std::memcpy(input.data(), Input.data(), sizeof(input));
    dispatch(Code, raw(Input.data(), InputBytes, 0x31016facu));
    Require(Output == input, "raw load through a TYPE 0 32_FLOAT V# did not read the buffer");
    dispatch(formattedLoad, strided(Input.data(), 0x31014facu));
    Require(Output == input, "formatted load through a TYPE 0 32_UINT V# did not read the buffer");
    dispatch(rawStore, raw(Output.data(), sizeof(Output), 0x31016facu));
    Require(all(Value), "raw store through a TYPE 0 32_FLOAT V# did not write every lane");
    dispatch(formattedStore, strided(Output.data(), 0x31016facu));
    Require(all(Value), "formatted store through a TYPE 0 32_FLOAT V# did not write every lane");
    dispatch(Code, raw(Input.data(), InputBytes, 0x31000facu));
    Require(all(0u), "raw load through a TYPE 0 FORMAT 0 V# did not return 0");
    dispatch(formattedLoad, strided(Input.data(), 0x31000facu));
    Require(all(0u), "formatted load through a TYPE 0 FORMAT 0 V# did not return 0");
    for (const auto word3 : {0x31000facu, 0x91000facu, 0x91016facu, 0x91b00facu, 0xd1000facu, 0xd1016facu}) {
        dispatch(rawStore, raw(Output.data(), sizeof(Output), word3));
        Require(all(Fill), "raw store through V# word 3 " + Hex(word3) + " wrote memory");
        dispatch(formattedStore, strided(Output.data(), word3));
        Require(all(Fill), "formatted store through V# word 3 " + Hex(word3) + " wrote memory");
    }
    dispatch(formattedStore, {0x02100b00u, 0xc3c00000u, 0x010dc1dfu, 0x91b00facu});
    Require(all(Fill), "store through an image T# read as a V# wrote memory");
    const auto rejects = [&](std::span<const std::uint32_t> code, std::uint32_t word3, const char* access) {
        bool rejected = false;
        try {
            dispatch(code, raw(Output.data(), sizeof(Output), word3));
        } catch (const std::runtime_error& error) {
            const auto message = std::string(error.what());
            Require(message.find("buffer descriptor uses an unsupported type") != std::string::npos && message.find("first used at pc 0x") != std::string::npos, std::string(access) + " through V# word 3 " + Hex(word3) + " failed for another reason: " + message);
            rejected = true;
        }
        Require(rejected, std::string(access) + " accepted V# word 3 " + Hex(word3));
    };
    for (const auto word3 : {0x71000facu, 0x71016facu, 0xb107ffacu, 0xf107ffacu}) {
        rejects(Code, word3, "a raw load");
        rejects(rawStore, word3, "a raw store");
        rejects(formattedStore, word3, "a formatted store");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t index = 0; index < InputBytes; ++index) Input[index] = static_cast<std::uint8_t>(index * 41u + 3u + (index >> 3u));
        Run(*device, false);
        Run(*device, true);
        Run(*device, false, true);
        Run(*device, true, true);
        ScalarReads(*device);
        DisabledDescriptors(*device);
        std::puts("buffer descriptor runtime tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
