#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "ImageTableAbi.hpp"
#include "BufferTableAbi.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/DescriptorBindingBuilder.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
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
        ShaderRecompiler::DescriptorValue descriptor{};
        std::copy(input.begin(), input.end(), descriptor.dwords.begin());
        descriptor.dwordCount = 4u;
        const auto expectedTail = test.records == 0u ? 0u : (test.stride == 0u ? test.records : test.stride * test.records) & 3u;
        Require(ShaderRecompiler::BufferSpecializationFields(descriptor, {}, 0u).tail == expectedTail, "scalar buffer size tail differs for stride " + std::to_string(test.stride) + " records " + std::to_string(test.records));
        device.Dispatch(result, 1, 1, 1);
        device.WaitIdle();
        const auto value = test.inBounds ? expected : 0u;
        Require(std::all_of(Output.begin(), Output.end(), [value](std::uint32_t word) { return word == value; }), "scalar buffer bounds differ for stride " + std::to_string(test.stride) + " records " + std::to_string(test.records));
    }
    std::uint32_t unalignedExpected = 0;
    std::memcpy(&unalignedExpected, Input.data() + 4u, sizeof(unalignedExpected));
    for (const auto& [base, records, value] : {std::tuple{Input.data() + 2u, 6u, 0u}, std::tuple{Input.data() + 2u, 8u, unalignedExpected}, std::tuple{static_cast<std::uint8_t*>(nullptr), 8u, 0u}}) {
        const auto input = BufferDescriptor(base, records);
        std::copy(input.begin(), input.end(), userData.begin());
        Output.fill(Fill);
        const auto result = ShaderRecompiler::Recompile(request);
        device.Dispatch(result, 1, 1, 1);
        device.WaitIdle();
        Require(std::all_of(Output.begin(), Output.end(), [value](std::uint32_t word) { return word == value; }), "scalar buffer read differs for base " + Hex(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base))) + " records " + std::to_string(records) + ": " + Hex(Output[0]));
    }
    constexpr std::array<std::uint32_t, 6> storeCode{0x34020082u, 0x7e0402ffu, 0x12345678u, 0xe0701000u, 0x80000201u, 0xbf810000u};
    auto oversized = BufferDescriptor(reinterpret_cast<const void*>(std::uintptr_t{0x10000000000u}), 0xffffffffu);
    oversized[1] |= 2u << 16u;
    std::copy(oversized.begin(), oversized.end(), userData.begin());
    for (const auto code : {std::span<const std::uint32_t>(scalarCode), std::span<const std::uint32_t>(Code)}) {
        request.shader.code = code;
        request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
        Output.fill(Fill);
        const auto readResult = ShaderRecompiler::Recompile(request);
        device.Dispatch(readResult, 1, 1, 1);
        device.WaitIdle();
        Require(std::all_of(Output.begin(), Output.end(), [](std::uint32_t word) { return word == 0u; }), "read through an oversized V# before any store returned " + Hex(Output[0]));
    }
    request.shader.code = storeCode;
    request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(storeCode.data());
    const auto storeResult = ShaderRecompiler::Recompile(request);
    device.Dispatch(storeResult, 1, 1, 1);
    device.WaitIdle();
    const auto null = BufferDescriptor(nullptr, 8u);
    std::copy(null.begin(), null.end(), userData.begin());
    request.shader.code = scalarCode;
    request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(scalarCode.data());
    Output.fill(Fill);
    const auto nullResult = ShaderRecompiler::Recompile(request);
    device.Dispatch(nullResult, 1, 1, 1);
    device.WaitIdle();
    Require(std::all_of(Output.begin(), Output.end(), [](std::uint32_t word) { return word == 0u; }), "scalar read through a null V# after a store through an oversized V# returned " + Hex(Output[0]));
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

alignas(256) static constexpr std::array<std::uint32_t, 18> CounterLoopCode{
    0x34020082u, 0x7e060280u, 0xbe880380u, 0x8f098408u,
    0xf4280300u, 0x12000000u, 0xbf8c0000u, 0xe0301000u,
    0x80030201u, 0xbf8c0000u, 0x4a060702u, 0x80088108u,
    0xbf0a8308u, 0xbf85fff5u, 0xe0701000u, 0x80010301u,
    0xbf8c3f70u, 0xbf810000u,
};

alignas(256) static constexpr std::array<std::uint32_t, 22> WaterfallCode{
    0x34020082u, 0x2c080083u, 0x7e060280u, 0xbe880380u,
    0xd7600009u, 0x00001104u, 0x8f0a8409u, 0xf4280300u,
    0x14000000u, 0xbf8c0000u, 0xe0301000u, 0x80030201u,
    0xbf8c0000u, 0x7d840809u, 0x02060503u, 0x80088808u,
    0xbf0aa008u, 0xbf85fff2u, 0xe0701000u, 0x80010301u,
    0xbf8c3f70u, 0xbf810000u,
};

alignas(256) std::array<std::uint32_t, Lanes> DynamicInputA{};
alignas(256) std::array<std::uint32_t, Lanes> DynamicInputB{};
alignas(256) std::array<std::uint32_t, Lanes> DynamicInputC{};
alignas(256) std::array<std::uint32_t, Lanes> DynamicInputD{};
alignas(256) std::array<std::uint32_t, 16> DynamicTable{};
const std::array<ShaderRecompiler::MemoryRegion, 1> DynamicMemory{{{reinterpret_cast<std::uintptr_t>(DynamicTable.data()), std::as_bytes(std::span(DynamicTable))}}};

std::size_t CountMainSpanCalls(std::span<const std::uint32_t> words);
bool HasSpirvCapability(std::span<const std::uint32_t> words, spv::Capability cap);
bool HasSpirvOp(std::span<const std::uint32_t> words, spv::Op targetOp);
bool HasSpirvConstant(std::span<const std::uint32_t> words, std::uint32_t value);

void DynamicDescriptorTable(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        DynamicInputA[lane] = 0x10000000u + (lane * 11u + 3u);
        DynamicInputB[lane] = 0x20000000u + (lane * 17u + 7u);
        DynamicInputC[lane] = 0x30000000u + (lane * 23u + 13u);
        DynamicInputD[lane] = 0x40000000u + (lane * 29u + 19u);
    }
    GuestAllocations::Mutation().Add(DynamicTable.data(), sizeof(DynamicTable), true, false, true);
    GuestAllocations::Mutation().Add(DynamicInputA.data(), sizeof(DynamicInputA), true, false, true);
    GuestAllocations::Mutation().Add(DynamicInputB.data(), sizeof(DynamicInputB), true, false, true);
    GuestAllocations::Mutation().Add(DynamicInputC.data(), sizeof(DynamicInputC), true, false, true);
    GuestAllocations::Mutation().Add(DynamicInputD.data(), sizeof(DynamicInputD), true, false, true);

    const auto setTableDescriptors = [](const void* row2Ptr, std::uint32_t row2Bytes) {
        const auto d0 = BufferDescriptor(DynamicInputA.data(), sizeof(DynamicInputA));
        auto d1 = BufferDescriptor(DynamicInputB.data(), 64u);
        d1[1] |= 4u << 16u;
        const auto d2 = BufferDescriptor(row2Ptr, row2Bytes);
        const std::array<std::uint32_t, 4> d3{0u, 0u, 0u, 0x31016facu};
        std::copy(d0.begin(), d0.end(), DynamicTable.begin());
        std::copy(d1.begin(), d1.end(), DynamicTable.begin() + 4);
        std::copy(d2.begin(), d2.end(), DynamicTable.begin() + 8);
        std::copy(d3.begin(), d3.end(), DynamicTable.begin() + 12);
    };
    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));

    std::array<std::uint32_t, 8> userData{};
    const auto tableDesc = BufferDescriptor(DynamicTable.data(), sizeof(DynamicTable));
    const auto outputDesc = BufferDescriptor(Output.data(), sizeof(Output));
    std::copy(tableDesc.begin(), tableDesc.end(), userData.begin());
    std::copy(outputDesc.begin(), outputDesc.end(), userData.begin() + 4);

    const bool tableOptIn = std::any_of(device.ComputeTarget(Lanes).supportedCapabilities.begin(), device.ComputeTarget(Lanes).supportedCapabilities.end(), [](std::uint32_t cap) {
        return cap == static_cast<std::uint32_t>(spv::CapabilityStorageBufferArrayNonUniformIndexing);
    });

    const auto dispatchAndVerify = [&](std::span<const std::uint32_t> code, const std::array<std::uint32_t, Lanes>& expected, const std::string& caseName) {
        Output.fill(Fill);
        auto request = Request(userData, device.ComputeTarget(Lanes), DynamicMemory);
        request.useCache = false;
        request.shader.code = code;
        request.shader.codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
        const auto result = ShaderRecompiler::Recompile(request);
        device.Dispatch(result, 1, 1, 1);
        device.WaitIdle();
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            Require(Output[lane] == expected[lane], "dynamic buffer table, " + caseName + ", lane " + std::to_string(lane) + " is " + Hex(Output[lane]) + ", expected " + Hex(expected[lane]));
        }
    };

    std::array<std::uint32_t, Lanes> expectedCounter{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const std::uint32_t bVal = (lane * 4u + 4u <= 64u) ? DynamicInputB[lane] : 0u;
        expectedCounter[lane] = DynamicInputA[lane] + bVal + DynamicInputC[lane];
    }
    dispatchAndVerify(CounterLoopCode, expectedCounter, "counter loop phi");

    setTableDescriptors(DynamicInputD.data(), sizeof(DynamicInputD));
    std::array<std::uint32_t, Lanes> expectedSnapshot{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const std::uint32_t bVal = (lane * 4u + 4u <= 64u) ? DynamicInputB[lane] : 0u;
        expectedSnapshot[lane] = DynamicInputA[lane] + bVal + DynamicInputD[lane];
    }
    dispatchAndVerify(CounterLoopCode, expectedSnapshot, "counter loop snapshot change");
    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));

    setTableDescriptors(reinterpret_cast<const std::uint8_t*>(DynamicInputC.data()) + 2u, sizeof(DynamicInputC) - 2u);
    std::array<std::uint32_t, Lanes> expectedMisaligned{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const std::uint32_t bVal = (lane * 4u + 4u <= 64u) ? DynamicInputB[lane] : 0u;
        std::uint32_t cVal = 0;
        if (lane * 4u + 4u <= sizeof(DynamicInputC) - 2u) {
            std::memcpy(&cVal, reinterpret_cast<const std::uint8_t*>(DynamicInputC.data()) + 2u + lane * 4u, sizeof(cVal));
        }
        expectedMisaligned[lane] = DynamicInputA[lane] + bVal + cVal;
    }
    dispatchAndVerify(CounterLoopCode, expectedMisaligned, "misaligned dynamic row");
    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));

    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC) + 64u);
    dispatchAndVerify(CounterLoopCode, expectedCounter, "partial mapping dynamic row counter loop");
    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));

    std::array<std::uint32_t, Lanes> expectedWaterfall{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const std::uint32_t row = lane >> 3u;
        if (row == 0u) expectedWaterfall[lane] = DynamicInputA[lane];
        else if (row == 1u) expectedWaterfall[lane] = DynamicInputB[lane];
        else if (row == 2u) expectedWaterfall[lane] = DynamicInputC[lane];
        else expectedWaterfall[lane] = 0u;
    }
    dispatchAndVerify(WaterfallCode, expectedWaterfall, "waterfall readlane phi");

    setTableDescriptors(reinterpret_cast<const std::uint8_t*>(DynamicInputC.data()) + 2u, sizeof(DynamicInputC) - 2u);
    std::array<std::uint32_t, Lanes> expectedWaterfallMisaligned{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const std::uint32_t row = lane >> 3u;
        if (row == 0u) expectedWaterfallMisaligned[lane] = DynamicInputA[lane];
        else if (row == 1u) expectedWaterfallMisaligned[lane] = DynamicInputB[lane];
        else if (row == 2u) {
            std::uint32_t val = 0;
            const auto offset = lane * 4u;
            if (offset + 4u <= sizeof(DynamicInputC) - 2u) {
                std::memcpy(&val, reinterpret_cast<const std::uint8_t*>(DynamicInputC.data()) + 2u + offset, sizeof(val));
            }
            expectedWaterfallMisaligned[lane] = val;
        } else {
            expectedWaterfallMisaligned[lane] = 0u;
        }
    }
    dispatchAndVerify(WaterfallCode, expectedWaterfallMisaligned, "misaligned dynamic row waterfall");
    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));

    alignas(256) auto relativeCounter = CounterLoopCode;
    alignas(256) auto relativeWaterfall = WaterfallCode;
    relativeCounter[7] |= 2u;
    relativeWaterfall[10] |= 2u;
    const auto readRelative = [](const auto& input, std::uint32_t lane, std::uint32_t bytes) {
        const auto offset = lane * 4u + 2u;
        std::uint32_t value = 0u;
        if (offset <= bytes && sizeof(value) <= bytes - offset) std::memcpy(&value, reinterpret_cast<const std::uint8_t*>(input.data()) + offset, sizeof(value));
        return value;
    };
    std::array<std::uint32_t, Lanes> expectedRelativeCounter{};
    std::array<std::uint32_t, Lanes> expectedRelativeWaterfall{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto a = readRelative(DynamicInputA, lane, sizeof(DynamicInputA));
        const auto b = readRelative(DynamicInputB, lane, 64u);
        const auto c = readRelative(DynamicInputC, lane, sizeof(DynamicInputC));
        expectedRelativeCounter[lane] = a + b + c;
        const auto row = lane >> 3u;
        expectedRelativeWaterfall[lane] = row == 0u ? a : row == 1u ? b : row == 2u ? c : 0u;
    }
    dispatchAndVerify(relativeCounter, expectedRelativeCounter, "unaligned relative counter address");
    dispatchAndVerify(relativeWaterfall, expectedRelativeWaterfall, "unaligned relative waterfall address");

    setTableDescriptors(DynamicInputC.data(), (1u << 27u) + 4u);
    std::array<std::uint32_t, Lanes> expectedOversized{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const std::uint32_t row = lane >> 3u;
        if (row == 0u) expectedOversized[lane] = DynamicInputA[lane];
        else if (row == 1u) expectedOversized[lane] = DynamicInputB[lane];
        else if (row == 2u) expectedOversized[lane] = DynamicInputC[lane];
        else expectedOversized[lane] = 0u;
    }
    dispatchAndVerify(WaterfallCode, expectedOversized, "oversized row BDA fallback reading data");
    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));

    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC) + 64u);
    dispatchAndVerify(WaterfallCode, expectedWaterfall, "partial mapping dynamic row waterfall");
    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));

    auto rejectedWord3 = BufferDescriptor(DynamicInputC.data(), sizeof(DynamicInputC));
    rejectedWord3[3] |= 1u << 30u;
    std::copy(rejectedWord3.begin(), rejectedWord3.end(), DynamicTable.begin() + 8);
    if (tableOptIn) {
        auto req = Request(userData, device.ComputeTarget(Lanes), DynamicMemory);
        req.useCache = false;
        req.shader.code = WaterfallCode;
        req.shader.codeAddress = reinterpret_cast<std::uintptr_t>(WaterfallCode.data());
        const auto handle = ShaderRecompiler::PrepareShader(req);
        ShaderRecompiler::SrtRuntime runtime{};
        runtime.userData = userData;
        runtime.readMemory = [](void*, std::uint64_t addr, std::uint32_t* value) {
            std::memcpy(value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), sizeof(*value));
            return true;
        };
        const auto capture = ShaderRecompiler::CaptureResources(req, runtime, *handle);
        const auto materialized = ShaderRecompiler::MaterializeShader(req, *capture, *handle);
        Require(materialized != nullptr, "dynamic buffer table: invalid type row materialized");
        const auto plan = ShaderRecompiler::GetResourcePlan(req);
        if (tableOptIn) {
            const auto resolved = ShaderRecompiler::ResourceMaterializer::ResolveImageTables(plan->info, capture->snapshot);
            Require(resolved.bufferRecordOffsets.size() >= 3u, "dynamic buffer table: buffer record offsets contain invalid row");
            const auto rejectedRowOffset = resolved.bufferRecordOffsets[2];
            const auto rejectedFlags = resolved.map[rejectedRowOffset + ShaderRecompiler::BufferTableAbi::BufferRecordFlags];
            Require((rejectedFlags & ShaderRecompiler::ImageTableAbi::PoisonFlag) != 0u, "dynamic buffer table: invalid type row flagged with PoisonFlag");
            const auto poisonIndex = rejectedFlags & ~ShaderRecompiler::ImageTableAbi::PoisonFlag;
            Require(poisonIndex < materialized->imageTablePoison.size(), "dynamic buffer table: poison index in bounds");
            Require(materialized->imageTablePoison[poisonIndex].reason == static_cast<std::uint32_t>(ShaderRecompiler::ImageTableAbi::PoisonReason::NotBuffer), "dynamic buffer table: invalid type row has NotBuffer poison reason");
        }
    }
    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));

    auto structuralRequest = Request(userData, device.ComputeTarget(Lanes), DynamicMemory);
    structuralRequest.useCache = false;
    structuralRequest.shader.code = CounterLoopCode;
    structuralRequest.shader.codeAddress = reinterpret_cast<std::uintptr_t>(CounterLoopCode.data());
    const auto plan = ShaderRecompiler::GetResourcePlan(structuralRequest);
    Require(plan != nullptr, "dynamic buffer table: plan is null");
    const auto dynamicLoad = std::find_if(plan->memoryInfo.begin(), plan->memoryInfo.end(), [](const auto& memory) {
        return memory.kind == ShaderRecompiler::ResourceKind::Buffer;
    });
    Require(dynamicLoad != plan->memoryInfo.end(), "dynamic buffer table: dynamic buffer load was not found in plan");

    const auto compiled = ShaderRecompiler::Recompile(structuralRequest);
    if (tableOptIn) {
        Require(!dynamicLoad->gpuDescriptor, "dynamic buffer table: dynamic V# descriptor table load lowered to BDA span instead of table-backed buffer");
        Require(std::any_of(plan->info.buffers.begin(), plan->info.buffers.end(), [](const auto& b) { return b.table != ShaderRecompiler::NoTable; }), "dynamic buffer table: buffer table assigned");
        Require(HasSpirvOp(compiled.spirv.Words(), spv::OpArrayLength), "dynamic buffer table: direct array length present");
        Require(HasSpirvConstant(compiled.spirv.Words(), ShaderRecompiler::BufferTableAbi::BdaFallbackFlag), "dynamic buffer table: BdaFallbackFlag constant present in SPIR-V");
    } else {
        Require(dynamicLoad->gpuDescriptor, "dynamic buffer table: fallback to BDA when opt-in disabled");
    }
    Require(CountMainSpanCalls(compiled.spirv.Words()) > 0u, "dynamic buffer table: BDA fallback path present in SPIR-V");

    GuestAllocations::Mutation().Remove(DynamicTable.data());
    GuestAllocations::Mutation().Remove(DynamicInputA.data());
    GuestAllocations::Mutation().Remove(DynamicInputB.data());
    GuestAllocations::Mutation().Remove(DynamicInputC.data());
    GuestAllocations::Mutation().Remove(DynamicInputD.data());
}

std::size_t CountMainSpanCalls(std::span<const std::uint32_t> words) {
    std::map<std::uint32_t, std::string> names;
    std::string function;
    std::size_t spanCalls = 0;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto length = words[cursor] >> 16u;
        if (length == 0 || length > words.size() - cursor) break;
        const auto op = words[cursor] & 0xffffu;
        if (op == spv::OpName) names[words[cursor + 1]] = reinterpret_cast<const char*>(&words[cursor + 2]);
        if (op == spv::OpFunction) function = names[words[cursor + 2]];
        if (op == spv::OpFunctionCall && function == "main") {
            const auto& callee = names[words[cursor + 3]];
            if (callee.starts_with("read_bda_span")) ++spanCalls;
        }
        cursor += length;
    }
    return spanCalls;
}

bool HasSpirvCapability(std::span<const std::uint32_t> words, spv::Capability cap) {
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto length = words[cursor] >> 16u;
        if (length == 0 || length > words.size() - cursor) break;
        const auto op = words[cursor] & 0xffffu;
        if (op == spv::OpCapability && words[cursor + 1] == static_cast<std::uint32_t>(cap)) return true;
        cursor += length;
    }
    return false;
}

bool HasSpirvOp(std::span<const std::uint32_t> words, spv::Op targetOp) {
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto length = words[cursor] >> 16u;
        if (length == 0 || length > words.size() - cursor) break;
        const auto op = words[cursor] & 0xffffu;
        if (op == static_cast<std::uint32_t>(targetOp)) return true;
        cursor += length;
    }
    return false;
}

bool HasSpirvConstant(std::span<const std::uint32_t> words, std::uint32_t value) {
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto length = words[cursor] >> 16u;
        if (length == 0 || length > words.size() - cursor) break;
        const auto op = words[cursor] & 0xffffu;
        if (op == spv::OpConstant && length >= 4 && words[cursor + 3] == value) return true;
        cursor += length;
    }
    return false;
}

ShaderRecompiler::SpirvTarget SyntheticTarget(bool bufferIndexing) {
    static const std::vector<std::uint32_t> baseCapabilities = {
        spv::CapabilityShader,
        spv::CapabilityInt64,
        spv::CapabilityPhysicalStorageBufferAddresses,
        spv::CapabilityGroupNonUniform,
        spv::CapabilityGroupNonUniformBallot,
        spv::CapabilityGroupNonUniformShuffle,
        spv::CapabilityGroupNonUniformArithmetic,
        spv::CapabilityShaderNonUniform,
        spv::CapabilitySampledImageArrayNonUniformIndexing,
        spv::CapabilityStorageBuffer8BitAccess,
    };
    static const std::vector<std::uint32_t> indexedCapabilities = {
        spv::CapabilityShader,
        spv::CapabilityInt64,
        spv::CapabilityPhysicalStorageBufferAddresses,
        spv::CapabilityGroupNonUniform,
        spv::CapabilityGroupNonUniformBallot,
        spv::CapabilityGroupNonUniformShuffle,
        spv::CapabilityGroupNonUniformArithmetic,
        spv::CapabilityShaderNonUniform,
        spv::CapabilitySampledImageArrayNonUniformIndexing,
        spv::CapabilityStorageBufferArrayNonUniformIndexing,
        spv::CapabilityStorageBufferArrayDynamicIndexing,
        spv::CapabilityStorageBuffer8BitAccess,
    };
    static const std::vector<std::string_view> extensions = {
        "SPV_KHR_physical_storage_buffer",
        "SPV_EXT_descriptor_indexing",
        "SPV_KHR_8bit_storage",
    };
    ShaderRecompiler::SpirvTarget target{};
    target.vulkanVersion = 0x00401000u;
    target.spirvVersion = 0x00010300u;
    target.subgroupSize = Lanes;
    target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    target.supportedCapabilities = bufferIndexing ? indexedCapabilities : baseCapabilities;
    target.supportedExtensions = extensions;
    target.fragmentShaderBarycentricEnabled = false;
    return target;
}

void RunCodegenOnly() {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        DynamicInputA[lane] = 0x10000000u + (lane * 11u + 3u);
        DynamicInputB[lane] = 0x20000000u + (lane * 17u + 7u);
        DynamicInputC[lane] = 0x30000000u + (lane * 23u + 13u);
        DynamicInputD[lane] = 0x40000000u + (lane * 29u + 19u);
    }
    GuestAllocations::Mutation().Add(DynamicTable.data(), sizeof(DynamicTable), true, false, true);
    GuestAllocations::Mutation().Add(DynamicInputA.data(), sizeof(DynamicInputA), true, false, true);
    GuestAllocations::Mutation().Add(DynamicInputB.data(), sizeof(DynamicInputB), true, false, true);
    GuestAllocations::Mutation().Add(DynamicInputC.data(), sizeof(DynamicInputC), true, false, true);
    GuestAllocations::Mutation().Add(DynamicInputD.data(), sizeof(DynamicInputD), true, false, true);

    const auto setTableDescriptors = [](const void* row2Ptr, std::uint32_t row2Bytes) {
        const auto d0 = BufferDescriptor(DynamicInputA.data(), sizeof(DynamicInputA));
        auto d1 = BufferDescriptor(DynamicInputB.data(), 64u);
        d1[1] |= 4u << 16u;
        const auto d2 = BufferDescriptor(row2Ptr, row2Bytes);
        const std::array<std::uint32_t, 4> d3{0u, 0u, 0u, 0x31016facu};
        std::copy(d0.begin(), d0.end(), DynamicTable.begin());
        std::copy(d1.begin(), d1.end(), DynamicTable.begin() + 4);
        std::copy(d2.begin(), d2.end(), DynamicTable.begin() + 8);
        std::copy(d3.begin(), d3.end(), DynamicTable.begin() + 12);
    };
    setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));

    std::array<std::uint32_t, 8> userData{};
    const auto tableDesc = BufferDescriptor(DynamicTable.data(), sizeof(DynamicTable));
    const auto outputDesc = BufferDescriptor(Output.data(), sizeof(Output));
    std::copy(tableDesc.begin(), tableDesc.end(), userData.begin());
    std::copy(outputDesc.begin(), outputDesc.end(), userData.begin() + 4);

    {
        auto req = Request(userData, SyntheticTarget(true), DynamicMemory);
        req.useCache = false;
        req.shader.code = CounterLoopCode;
        req.shader.codeAddress = reinterpret_cast<std::uintptr_t>(CounterLoopCode.data());
        const auto plan = ShaderRecompiler::GetResourcePlan(req);
        Require(plan != nullptr, "codegen: CounterLoopCode plan is null");
        const auto dynamicLoad = std::find_if(plan->memoryInfo.begin(), plan->memoryInfo.end(), [](const auto& memory) {
            return memory.kind == ShaderRecompiler::ResourceKind::Buffer;
        });
        Require(dynamicLoad != plan->memoryInfo.end(), "codegen: dynamic load not found in plan");
        Require(!dynamicLoad->gpuDescriptor, "codegen: dynamic load has gpuDescriptor true when indexing supported");
        Require(std::any_of(plan->info.buffers.begin(), plan->info.buffers.end(), [](const auto& b) { return b.table != ShaderRecompiler::NoTable; }), "codegen: buffer table not assigned");
        const auto recompiled = ShaderRecompiler::Recompile(req);
        Require(HasSpirvOp(recompiled.spirv.Words(), spv::OpArrayLength), "codegen: direct table access array length check present");
        Require(CountMainSpanCalls(recompiled.spirv.Words()) > 0u, "codegen: BDA fallback path present in CounterLoopCode");
        Require(HasSpirvConstant(recompiled.spirv.Words(), ShaderRecompiler::BufferTableAbi::BdaFallbackFlag), "codegen: BdaFallbackFlag present in SPIR-V");
    }

    {
        auto req = Request(userData, SyntheticTarget(true), DynamicMemory);
        req.useCache = false;
        req.shader.code = CounterLoopCode;
        req.shader.codeAddress = reinterpret_cast<std::uintptr_t>(CounterLoopCode.data());
        req.context.compute = ShaderRecompiler::ShaderComputeStageInfo{{Lanes * 2u, 1u, 1u}, 0u, {false, false, false}, false, 1};
        const auto plan = ShaderRecompiler::GetResourcePlan(req);
        Require(plan != nullptr, "codegen multi-wave: CounterLoopCode plan is null");
        const auto dynamicLoad = std::find_if(plan->memoryInfo.begin(), plan->memoryInfo.end(), [](const auto& memory) {
            return memory.kind == ShaderRecompiler::ResourceKind::Buffer;
        });
        Require(dynamicLoad != plan->memoryInfo.end(), "codegen multi-wave: dynamic load not found in plan");
        Require(!dynamicLoad->gpuDescriptor, "codegen multi-wave: dynamic load lowered to BDA span");
        Require(std::any_of(plan->info.buffers.begin(), plan->info.buffers.end(), [](const auto& b) { return b.table != ShaderRecompiler::NoTable; }), "codegen multi-wave: buffer table not assigned");
        const auto recompiled = ShaderRecompiler::Recompile(req);
        Require(HasSpirvCapability(recompiled.spirv.Words(), spv::CapabilityStorageBufferArrayNonUniformIndexing), "codegen multi-wave: storage buffer array indexing capability present");
        Require(HasSpirvOp(recompiled.spirv.Words(), spv::OpArrayLength), "codegen multi-wave: direct array length present");
        Require(CountMainSpanCalls(recompiled.spirv.Words()) > 0u, "codegen multi-wave: BDA fallback path present in CounterLoopCode");
        Require(HasSpirvConstant(recompiled.spirv.Words(), ShaderRecompiler::BufferTableAbi::BdaFallbackFlag), "codegen multi-wave: BdaFallbackFlag present in SPIR-V");
    }

    {
        auto req = Request(userData, SyntheticTarget(true), DynamicMemory);
        req.useCache = false;
        req.shader.code = WaterfallCode;
        req.shader.codeAddress = reinterpret_cast<std::uintptr_t>(WaterfallCode.data());
        const auto plan = ShaderRecompiler::GetResourcePlan(req);
        Require(plan != nullptr, "codegen: WaterfallCode plan is null");
        const auto dynamicLoad = std::find_if(plan->memoryInfo.begin(), plan->memoryInfo.end(), [](const auto& memory) {
            return memory.kind == ShaderRecompiler::ResourceKind::Buffer;
        });
        Require(dynamicLoad != plan->memoryInfo.end(), "codegen: waterfall dynamic load not found in plan");
        Require(!dynamicLoad->gpuDescriptor, "codegen: waterfall dynamic load has gpuDescriptor true when indexing supported");
        Require(std::any_of(plan->info.buffers.begin(), plan->info.buffers.end(), [](const auto& b) { return b.table != ShaderRecompiler::NoTable; }), "codegen: waterfall buffer table not assigned");
        const auto recompiled = ShaderRecompiler::Recompile(req);
        Require(HasSpirvOp(recompiled.spirv.Words(), spv::OpArrayLength), "codegen: waterfall direct array length present");
        Require(CountMainSpanCalls(recompiled.spirv.Words()) > 0u, "codegen: waterfall BDA fallback path present");
    }

    {
        auto req = Request(userData, SyntheticTarget(false), DynamicMemory);
        req.useCache = false;
        req.shader.code = CounterLoopCode;
        req.shader.codeAddress = reinterpret_cast<std::uintptr_t>(CounterLoopCode.data());
        const auto plan = ShaderRecompiler::GetResourcePlan(req);
        Require(plan != nullptr, "codegen: fallback plan is null");
        const auto dynamicLoad = std::find_if(plan->memoryInfo.begin(), plan->memoryInfo.end(), [](const auto& memory) {
            return memory.kind == ShaderRecompiler::ResourceKind::Buffer;
        });
        Require(dynamicLoad != plan->memoryInfo.end(), "codegen: fallback dynamic load not found");
        Require(dynamicLoad->gpuDescriptor, "codegen: dynamic load should fall back to BDA without indexing capability");
        const auto recompiled = ShaderRecompiler::Recompile(req);
        Require(CountMainSpanCalls(recompiled.spirv.Words()) > 0u, "codegen: read_bda_span expected when falling back to BDA");
    }

    {
        setTableDescriptors(reinterpret_cast<const std::uint8_t*>(DynamicInputC.data()) + 2u, sizeof(DynamicInputC) - 2u);
        auto req = Request(userData, SyntheticTarget(true), DynamicMemory);
        req.useCache = false;
        req.shader.code = CounterLoopCode;
        req.shader.codeAddress = reinterpret_cast<std::uintptr_t>(CounterLoopCode.data());
        const auto plan = ShaderRecompiler::GetResourcePlan(req);
        Require(plan != nullptr, "codegen: misaligned plan is null");
        const auto recompiled = ShaderRecompiler::Recompile(req);
        Require(CountMainSpanCalls(recompiled.spirv.Words()) > 0u, "codegen: misaligned dynamic row has BDA fallback");
        const auto handle = ShaderRecompiler::PrepareShader(req);
        ShaderRecompiler::SrtRuntime runtime{};
        runtime.userData = userData;
        runtime.readMemory = [](void*, std::uint64_t addr, std::uint32_t* value) {
            std::memcpy(value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), sizeof(*value));
            return true;
        };
        const auto capture = ShaderRecompiler::CaptureResources(req, runtime, *handle);
        const auto resolved = ShaderRecompiler::ResourceMaterializer::ResolveImageTables(plan->info, capture->snapshot);
        Require(resolved.bufferRecordOffsets.size() >= 3u, "codegen: misaligned row present in record offsets");
        const auto misalignedFlags = resolved.map[resolved.bufferRecordOffsets[2] + ShaderRecompiler::BufferTableAbi::BufferRecordFlags];
        Require((misalignedFlags & ShaderRecompiler::BufferTableAbi::BdaFallbackFlag) != 0u, "codegen: misaligned dynamic row has BdaFallbackFlag set");
        setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));
    }

    {
        setTableDescriptors(DynamicInputC.data(), static_cast<std::uint32_t>(ShaderRecompiler::BufferTableAbi::MaxDirectBufferBytes));
        auto req = Request(userData, SyntheticTarget(true), DynamicMemory);
        req.useCache = false;
        req.shader.code = WaterfallCode;
        req.shader.codeAddress = reinterpret_cast<std::uintptr_t>(WaterfallCode.data());
        const auto handle = ShaderRecompiler::PrepareShader(req);
        ShaderRecompiler::SrtRuntime runtime{};
        runtime.userData = userData;
        runtime.readMemory = [](void*, std::uint64_t addr, std::uint32_t* value) {
            std::memcpy(value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), sizeof(*value));
            return true;
        };
        const auto capture = ShaderRecompiler::CaptureResources(req, runtime, *handle);
        const auto materialized = ShaderRecompiler::MaterializeShader(req, *capture, *handle);
        Require(materialized != nullptr, "codegen: boundary 128MB buffer row materialized");
        const auto bufferBinding = std::find_if(materialized->bindings.begin(), materialized->bindings.end(), [](const auto& b) {
            return b.kind == ShaderRecompiler::DescriptorKind::StorageBuffer && b.role == ShaderRecompiler::DescriptorRole::GuestBuffers;
        });
        Require(bufferBinding != materialized->bindings.end(), "codegen: storage buffer binding found");
        Require(bufferBinding->count == bufferBinding->firstTableBuffer + ShaderRecompiler::BufferTableAbi::MaxBufferElements && bufferBinding->guestDescriptor.size() == bufferBinding->count * 4u, "codegen: direct table binding retains its padded descriptor capacity");

        const auto plan = ShaderRecompiler::GetResourcePlan(req);
        const auto resolved = ShaderRecompiler::ResourceMaterializer::ResolveImageTables(plan->info, capture->snapshot);
        Require(!resolved.bufferRecordOffsets.empty(), "codegen: buffer record offsets present in resolved tables");
        const auto directRowOffset = resolved.bufferRecordOffsets.back();
        const auto directFlags = resolved.map[directRowOffset + ShaderRecompiler::BufferTableAbi::BufferRecordFlags];
        Require((directFlags & ShaderRecompiler::BufferTableAbi::BdaFallbackFlag) == 0u, "codegen: 128MB row without fallback flag");

        setTableDescriptors(DynamicInputC.data(), static_cast<std::uint32_t>(ShaderRecompiler::BufferTableAbi::MaxDirectBufferBytes) + 4u);
        const auto captureOversized = ShaderRecompiler::CaptureResources(req, runtime, *handle);
        const auto materializedOversized = ShaderRecompiler::MaterializeShader(req, *captureOversized, *handle);
        Require(materializedOversized != nullptr, "codegen: oversized buffer row materialized without throwing");
        const auto bufferBindingOversized = std::find_if(materializedOversized->bindings.begin(), materializedOversized->bindings.end(), [](const auto& b) {
            return b.kind == ShaderRecompiler::DescriptorKind::StorageBuffer && b.role == ShaderRecompiler::DescriptorRole::GuestBuffers;
        });
        Require(bufferBindingOversized != materializedOversized->bindings.end(), "codegen: storage buffer binding found for oversized");
        Require(bufferBindingOversized->count == bufferBinding->count && bufferBindingOversized->guestDescriptor.size() == bufferBinding->guestDescriptor.size(), "codegen: fallback row retains the fixed descriptor layout");

        const auto resolvedOversized = ShaderRecompiler::ResourceMaterializer::ResolveImageTables(plan->info, captureOversized->snapshot);
        const auto oversizedRowOffset = resolvedOversized.bufferRecordOffsets.back();
        const auto oversizedFlags = resolvedOversized.map[oversizedRowOffset + ShaderRecompiler::BufferTableAbi::BufferRecordFlags];
        Require((oversizedFlags & ShaderRecompiler::BufferTableAbi::BdaFallbackFlag) != 0u, "codegen: oversized row has BdaFallbackFlag set");

        setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));
    }

    {
        alignas(256) std::array<std::uint32_t, 68 * 4> bigTable{};
        for (std::uint32_t i = 0; i < 68; ++i) {
            const auto desc = BufferDescriptor(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x10000000u + i * 0x1000u)), 256u);
            std::copy(desc.begin(), desc.end(), bigTable.begin() + i * 4);
        }
        std::array<std::uint32_t, 8> bigUserData{};
        const auto bigTableDesc = BufferDescriptor(bigTable.data(), sizeof(bigTable));
        const auto outputDesc = BufferDescriptor(Output.data(), sizeof(Output));
        std::copy(bigTableDesc.begin(), bigTableDesc.end(), bigUserData.begin());
        std::copy(outputDesc.begin(), outputDesc.end(), bigUserData.begin() + 4);

        const std::array<ShaderRecompiler::MemoryRegion, 1> bigMemory{{{reinterpret_cast<std::uintptr_t>(bigTable.data()), std::as_bytes(std::span(bigTable))}}};
        auto req = Request(bigUserData, SyntheticTarget(true), bigMemory);
        req.useCache = false;
        req.shader.code = WaterfallCode;
        req.shader.codeAddress = reinterpret_cast<std::uintptr_t>(WaterfallCode.data());
        const auto handle = ShaderRecompiler::PrepareShader(req);
        ShaderRecompiler::SrtRuntime runtime{};
        runtime.userData = bigUserData;
        runtime.readMemory = [](void*, std::uint64_t addr, std::uint32_t* value) {
            std::memcpy(value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), sizeof(*value));
            return true;
        };
        const auto capture = ShaderRecompiler::CaptureResources(req, runtime, *handle);
        const auto materialized = ShaderRecompiler::MaterializeShader(req, *capture, *handle);
        Require(materialized != nullptr, "codegen: big table materialized");
        const auto plan = ShaderRecompiler::GetResourcePlan(req);
        const auto resolved = ShaderRecompiler::ResourceMaterializer::ResolveImageTables(plan->info, capture->snapshot);
        Require(resolved.bufferElements.size() == ShaderRecompiler::BufferTableAbi::MaxBufferElements, "codegen: direct buffer elements bounded by MaxBufferElements");
        for (std::size_t i = ShaderRecompiler::BufferTableAbi::MaxBufferElements; i < resolved.bufferRecordOffsets.size(); ++i) {
            const auto offset = resolved.bufferRecordOffsets[i];
            const auto flags = resolved.map[offset + ShaderRecompiler::BufferTableAbi::BufferRecordFlags];
            Require((flags & ShaderRecompiler::BufferTableAbi::BdaFallbackFlag) != 0u, "codegen: capacity overflow row has BdaFallbackFlag set");
        }
        for (const auto offset : resolved.bufferRecordOffsets) {
            Require(offset + ShaderRecompiler::BufferTableAbi::BufferRecordWords <= resolved.map.size(), "codegen: buffer record offset in bounds");
        }
    }

    {
        auto rejectedDesc = BufferDescriptor(DynamicInputC.data(), sizeof(DynamicInputC));
        rejectedDesc[3] |= 1u << 30u;
        std::copy(rejectedDesc.begin(), rejectedDesc.end(), DynamicTable.begin() + 8);
        auto req = Request(userData, SyntheticTarget(true), DynamicMemory);
        req.useCache = false;
        req.shader.code = WaterfallCode;
        req.shader.codeAddress = reinterpret_cast<std::uintptr_t>(WaterfallCode.data());
        const auto handle = ShaderRecompiler::PrepareShader(req);
        ShaderRecompiler::SrtRuntime runtime{};
        runtime.userData = userData;
        runtime.readMemory = [](void*, std::uint64_t addr, std::uint32_t* value) {
            std::memcpy(value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), sizeof(*value));
            return true;
        };
        const auto capture = ShaderRecompiler::CaptureResources(req, runtime, *handle);
        const auto materialized = ShaderRecompiler::MaterializeShader(req, *capture, *handle);
        Require(materialized != nullptr, "codegen: invalid type row materialized");
        const auto plan = ShaderRecompiler::GetResourcePlan(req);
        const auto resolved = ShaderRecompiler::ResourceMaterializer::ResolveImageTables(plan->info, capture->snapshot);
        Require(resolved.bufferRecordOffsets.size() >= 3u, "codegen: buffer record offsets contain invalid row");
        const auto rejectedRowOffset = resolved.bufferRecordOffsets[2];
        const auto rejectedFlags = resolved.map[rejectedRowOffset + ShaderRecompiler::BufferTableAbi::BufferRecordFlags];
        Require((rejectedFlags & ShaderRecompiler::ImageTableAbi::PoisonFlag) != 0u, "codegen: invalid type row flagged with PoisonFlag");
        const auto poisonIndex = rejectedFlags & ~ShaderRecompiler::ImageTableAbi::PoisonFlag;
        Require(poisonIndex < materialized->imageTablePoison.size(), "codegen: poison index in bounds");
        Require(materialized->imageTablePoison[poisonIndex].reason == static_cast<std::uint32_t>(ShaderRecompiler::ImageTableAbi::PoisonReason::NotBuffer), "codegen: invalid type row has NotBuffer poison reason");
        const auto recompiled = ShaderRecompiler::Recompile(req);
        constexpr std::uint32_t notBufferPoison = ShaderRecompiler::ImageTableAbi::PoisonFlag | static_cast<std::uint32_t>(ShaderRecompiler::ImageTableAbi::PoisonReason::NotBuffer);
        const auto words = recompiled.spirv.Words();
        Require(std::ranges::find(words, notBufferPoison) != words.end(), "codegen: SPIR-V contains table descriptor validation rejection logic");
        setTableDescriptors(DynamicInputC.data(), sizeof(DynamicInputC));
    }

    GuestAllocations::Mutation().Remove(DynamicTable.data());
    GuestAllocations::Mutation().Remove(DynamicInputA.data());
    GuestAllocations::Mutation().Remove(DynamicInputB.data());
    GuestAllocations::Mutation().Remove(DynamicInputC.data());
    GuestAllocations::Mutation().Remove(DynamicInputD.data());
}

}

int main(int argc, char** argv) {
    try {
        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--codegen-only") {
                RunCodegenOnly();
                std::puts("buffer descriptor codegen tests passed");
                return 0;
            }
        }
        RunCodegenOnly();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t index = 0; index < InputBytes; ++index) Input[index] = static_cast<std::uint8_t>(index * 41u + 3u + (index >> 3u));
        Run(*device, false);
        Run(*device, true);
        Run(*device, false, true);
        Run(*device, true, true);
        ScalarReads(*device);
        DisabledDescriptors(*device);
        DynamicDescriptorTable(*device);
        std::puts("buffer descriptor runtime tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
