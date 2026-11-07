#include "BdaShader.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>
#include <array>

namespace {

struct ScanWords {
    std::vector<std::uint32_t> words;
    void Op(spv::Op opcode, std::initializer_list<std::uint32_t> operands) {
        words.push_back((static_cast<std::uint32_t>(operands.size() + 1u) << 16u) | static_cast<std::uint32_t>(opcode));
        words.insert(words.end(), operands.begin(), operands.end());
    }
    void OpString(spv::Op opcode, std::initializer_list<std::uint32_t> operands, const char* text) {
        std::vector<std::uint32_t> packed((std::strlen(text) + 4u) / 4u, 0u);
        std::memcpy(packed.data(), text, std::strlen(text));
        words.push_back((static_cast<std::uint32_t>(operands.size() + packed.size() + 1u) << 16u) | static_cast<std::uint32_t>(opcode));
        words.insert(words.end(), operands.begin(), operands.end());
        words.insert(words.end(), packed.begin(), packed.end());
    }
};

enum FaultScanId : std::uint32_t {
    FaultVoid = 1, FaultVoidFunction, FaultUint, FaultWords, FaultBlock, FaultBlockPointer, FaultWordPointer, FaultHelperFunction, FaultVariable, FaultZero, FaultReportLast, FaultOverflowWord, FaultScope, FaultSemantics, FaultMain, FaultMainLabel, FaultHelper, FaultHelperParameter, FaultHelperLabel, FaultBody,
};

bool ScanFaultReport(const std::function<void(ScanWords&)>& main, const std::function<void(ScanWords&)>& after = {}) {
    ScanWords module{{spv::MagicNumber, 0x00010300u, 0u, 100u, 0u}};
    module.Op(spv::OpCapability, {spv::CapabilityShader});
    module.Op(spv::OpMemoryModel, {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
    module.OpString(spv::OpEntryPoint, {spv::ExecutionModelGLCompute, FaultMain}, "main");
    module.Op(spv::OpDecorate, {FaultVariable, spv::DecorationDescriptorSet, 0u});
    module.Op(spv::OpDecorate, {FaultVariable, spv::DecorationBinding, 5u});
    module.Op(spv::OpTypeVoid, {FaultVoid});
    module.Op(spv::OpTypeFunction, {FaultVoidFunction, FaultVoid});
    module.Op(spv::OpTypeInt, {FaultUint, 32u, 0u});
    module.Op(spv::OpTypeRuntimeArray, {FaultWords, FaultUint});
    module.Op(spv::OpTypeStruct, {FaultBlock, FaultWords});
    module.Op(spv::OpTypePointer, {FaultBlockPointer, spv::StorageClassStorageBuffer, FaultBlock});
    module.Op(spv::OpTypePointer, {FaultWordPointer, spv::StorageClassStorageBuffer, FaultUint});
    module.Op(spv::OpTypeFunction, {FaultHelperFunction, FaultVoid, FaultBlockPointer});
    module.Op(spv::OpConstant, {FaultUint, FaultZero, 0u});
    module.Op(spv::OpConstant, {FaultUint, FaultReportLast, 7u});
    module.Op(spv::OpConstant, {FaultUint, FaultOverflowWord, ShaderRecompiler::BdaAbi::WrittenOverflowWord});
    module.Op(spv::OpConstant, {FaultUint, FaultScope, spv::ScopeDevice});
    module.Op(spv::OpConstant, {FaultUint, FaultSemantics, 0u});
    module.Op(spv::OpVariable, {FaultBlockPointer, FaultVariable, spv::StorageClassStorageBuffer});
    module.Op(spv::OpFunction, {FaultVoid, FaultMain, spv::FunctionControlMaskNone, FaultVoidFunction});
    module.Op(spv::OpLabel, {FaultMainLabel});
    main(module);
    module.Op(spv::OpReturn, {});
    module.Op(spv::OpFunctionEnd, {});
    if (after) after(module);
    return AgcDriver::Graphics::SpirvFaultReportOnly(module.words, 0, 5);
}

void RunFaultReportScanTests() {
    using AgcDriver::Graphics::Require;
    Require(ScanFaultReport([](ScanWords& m) {
        m.Op(spv::OpAccessChain, {FaultWordPointer, FaultBody, FaultVariable, FaultZero, FaultZero});
        m.Op(spv::OpAtomicCompareExchange, {FaultUint, FaultBody + 1u, FaultBody, FaultScope, FaultSemantics, FaultSemantics, FaultReportLast, FaultZero});
        m.Op(spv::OpAccessChain, {FaultWordPointer, FaultBody + 2u, FaultVariable, FaultZero, FaultReportLast});
        m.Op(spv::OpStore, {FaultBody + 2u, FaultZero});
        m.Op(spv::OpAtomicOr, {FaultUint, FaultBody + 3u, FaultBody + 2u, FaultScope, FaultSemantics, FaultReportLast});
        m.Op(spv::OpLoad, {FaultUint, FaultBody + 4u, FaultBody + 2u});
    }), "fault scan: loads, stores and atomics on the report words were taken for writes past the report");
    Require(!ScanFaultReport([](ScanWords& m) {
        m.Op(spv::OpAccessChain, {FaultWordPointer, FaultBody, FaultVariable, FaultZero, FaultOverflowWord});
        m.Op(spv::OpStore, {FaultBody, FaultZero});
    }), "fault scan: a store to the written-page overflow word was missed");
    Require(!ScanFaultReport([](ScanWords& m) {
        m.Op(spv::OpIAdd, {FaultUint, FaultBody, FaultZero, FaultReportLast});
        m.Op(spv::OpAccessChain, {FaultWordPointer, FaultBody + 1u, FaultVariable, FaultZero, FaultBody});
        m.Op(spv::OpStore, {FaultBody + 1u, FaultZero});
    }), "fault scan: a store at a computed word was taken for a report store");
    Require(!ScanFaultReport([](ScanWords& m) {
        m.Op(spv::OpAccessChain, {FaultWordPointer, FaultBody, FaultVariable, FaultZero, FaultZero});
        m.Op(spv::OpCopyObject, {FaultWordPointer, FaultBody + 1u, FaultBody});
        m.Op(spv::OpStore, {FaultBody + 1u, FaultZero});
    }), "fault scan: a copied pointer to a report word was not followed");
    Require(!ScanFaultReport([](ScanWords& m) { m.Op(spv::OpFunctionCall, {FaultVoid, FaultBody, FaultHelper, FaultVariable}); }, [](ScanWords& m) {
        m.Op(spv::OpFunction, {FaultVoid, FaultHelper, spv::FunctionControlMaskNone, FaultHelperFunction});
        m.Op(spv::OpFunctionParameter, {FaultBlockPointer, FaultHelperParameter});
        m.Op(spv::OpLabel, {FaultHelperLabel});
        m.Op(spv::OpAccessChain, {FaultWordPointer, FaultBody + 1u, FaultHelperParameter, FaultZero, FaultOverflowWord});
        m.Op(spv::OpStore, {FaultBody + 1u, FaultZero});
        m.Op(spv::OpReturn, {});
        m.Op(spv::OpFunctionEnd, {});
    }), "fault scan: the fault buffer handed to a function was not followed");
}


template<typename TAction>
void reject(TAction action, const char* expected) {
    try { action(); }
    catch (const std::runtime_error& error) {
        AgcDriver::Graphics::Require(std::string(error.what()).find(expected) != std::string::npos, std::string("unexpected contract error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("missing contract rejection: ") + expected);
}

std::uint32_t CallSites(const std::vector<std::uint32_t>& spirv, const char* name) {
    std::uint32_t function = 0;
    std::uint32_t calls = 0;
    for (std::size_t at = 5; at < spirv.size() && (spirv[at] >> 16u) != 0u; at += spirv[at] >> 16u) {
        const auto opcode = spirv[at] & 0xffffu;
        if (opcode == spv::OpName && std::strcmp(reinterpret_cast<const char*>(&spirv[at + 2u]), name) == 0) function = spirv[at + 1u];
        if (opcode == spv::OpFunctionCall && function != 0u && spirv[at + 3u] == function) ++calls;
    }
    return calls;
}

}

void RunBdaContractTests() {
    using namespace ShaderRecompiler;
    RunFaultReportScanTests();
    const std::array<std::uint32_t, 5> headerOnly{spv::MagicNumber, 0x00010300u, 0u, 1u, 0u};
    const std::array<std::uint32_t, 2> truncated{spv::MagicNumber, 0u};
    auto unparsed = MakeBdaStoreTestShader(0x1000, 7, 32, false);
    unparsed[5] |= 0xffff0000u;
    using AgcDriver::Graphics::SpirvFaultReportOnly;
    AgcDriver::Graphics::Require(SpirvFaultReportOnly(MakeBdaTestShader(0x1000, 32), 0, 1) && SpirvFaultReportOnly(MakeBdaDwordReadTestShader(0x1000, 4, false, false), 0, 1) && SpirvFaultReportOnly(MakeBdaFaultSequenceTestShader({BdaAbi::FaultReason::Unmapped, BdaAbi::FaultReason::Unaligned}), 0, 1), "a BDA read program, which writes only the fault report, was given a full fault buffer");
    AgcDriver::Graphics::Require(!SpirvFaultReportOnly(MakeBdaStoreTestShader(0x1000, 7, 32, false), 0, 1) && !SpirvFaultReportOnly(MakeBdaWriteTestShader(0x1000, 7, 32, false), 0, 1), "a BDA store program, which records written pages, was given a report-only fault buffer");
    AgcDriver::Graphics::Require(!SpirvFaultReportOnly(MakeBdaTestShader(0x1000, 32), 0, 3) && !SpirvFaultReportOnly(headerOnly, 0, 1) && !SpirvFaultReportOnly(truncated, 0, 1) && !SpirvFaultReportOnly(unparsed, 0, 1), "a module without the fault buffer, or one that cannot be read, was taken for a report-only one");
    const std::array<std::uint32_t, 3> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    SpirvTargetOptions target{0x00401000u, 0x00010300u, 32, BdaAbi::Version, capabilities, extensions};
    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    program.Info().usesDma = true;
    ValidateBdaTarget(program, target);
    target.bdaAbiVersion = 0;
    reject([&] { ValidateBdaTarget(program, target); }, "ABI version");
    target.bdaAbiVersion = BdaAbi::Version;
    target.supportedCapabilities = {};
    reject([&] { ValidateBdaTarget(program, target); }, "capability");
    target.supportedCapabilities = capabilities;
    target.supportedExtensions = {};
    reject([&] { ValidateBdaTarget(program, target); }, "extension");
    target.supportedExtensions = extensions;
    auto& block = program.CreateBlock();
    block.AppendInstruction(&program.CreateValue(IrOpcode::Barrier, IrType::Void));
    program.BlockOrder().push_back(&block);
    ValidateBdaTarget(program, target);
    AgcDriver::Graphics::Require(!BdaInvocationsMayStop(program), "a BDA program with barriers must keep its invocations running");
    for (const auto opcode : {IrOpcode::ReadLane, IrOpcode::WriteLane}) {
        IrProgram lanes;
        lanes.Resources().stage = IrShaderStage::Pixel;
        auto& laneBlock = lanes.CreateBlock();
        laneBlock.AppendInstruction(&lanes.CreateValue(opcode, IrType::U32));
        lanes.BlockOrder().push_back(&laneBlock);
        AgcDriver::Graphics::Require(!BdaInvocationsMayStop(lanes), std::string("a pixel program with ") + (opcode == IrOpcode::ReadLane ? "v_readlane" : "v_writelane") + " must keep its invocations running: a waterfall over a mask that holds a stopped lane never clears its bit");
    }
    IrProgram plain;
    plain.Resources().stage = IrShaderStage::Pixel;
    auto& plainBlock = plain.CreateBlock();
    plainBlock.AppendInstruction(&plain.CreateValue(IrOpcode::ReadFirstLane, IrType::U32));
    plain.BlockOrder().push_back(&plainBlock);
    AgcDriver::Graphics::Require(BdaInvocationsMayStop(plain), "a BDA program with only v_readfirstlane must still stop a faulting invocation");
    IrProgram computeLanes;
    computeLanes.Resources().stage = IrShaderStage::Compute;
    auto& computeBlock = computeLanes.CreateBlock();
    computeBlock.AppendInstruction(&computeLanes.CreateValue(IrOpcode::ReadLane, IrType::U32));
    computeLanes.BlockOrder().push_back(&computeBlock);
    AgcDriver::Graphics::Require(BdaInvocationsMayStop(computeLanes), "a compute program with v_readlane and no barrier must still stop a faulting invocation");
    program.Resources().stage = IrShaderStage::TessellationControl;
    ValidateBdaTarget(program, target);
    program.Resources().stage = IrShaderStage::Compute;
    RecompileRequest request{};
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.fragmentShaderBarycentricEnabled = false;
    const RequestSerializer serializer;
    const auto encoded = serializer.Serialize(request);
    const auto decoded = serializer.Deserialize(encoded);
    AgcDriver::Graphics::Require(decoded.request.target.bdaAbiVersion == BdaAbi::Version && decoded.request.target.supportedCapabilities.size() == capabilities.size() && decoded.request.target.supportedExtensions[1] == extensions[1], "BDA request serialization changed target contract");
    auto invalid = encoded;
    invalid[0] = invalid[0] == 'A' ? 'B' : 'A';
    reject([&] { static_cast<void>(serializer.Deserialize(invalid)); }, "request signature");
    reject([&] { static_cast<void>(serializer.Deserialize("NVNQQWMAAAA=")); }, "serialization version");
    const auto ordered = [](std::initializer_list<BdaAbi::Range> ranges) { return BdaAbi::RangesOrdered(ranges.begin(), ranges.size()); };
    AgcDriver::Graphics::Require(ordered({}) && ordered({{0x1000, 0x2000, 1, 1, 0}, {0x2000, 0x2000, 1, 1, 0}, {0x2000, 0x3000, 1, 1, 0}, {0x4000, 0x5000, 1, 1, 0}}), "ascending BDA ranges were refused");
    AgcDriver::Graphics::Require(ordered({{0x1000, 0x3000, 1, 1, 0}, {0x2f00, 0x3100, 1, 1, 0}}), "BDA ranges ascending by begin and overlapping were refused");
    AgcDriver::Graphics::Require(!ordered({{0x4000, 0x5000, 1, 1, 0}, {0x1000, 0x2000, 1, 1, 0}}), "unsorted BDA ranges were accepted");
    for (const std::uint32_t bits : {8u, 16u, 32u}) {
        const auto shader = MakeBdaWriteTestShader(0x7fff1235fffdULL, 0xa1b2c3d4u, bits, false);
        const auto lookups = CallSites(shader, "get_bda_write_pointer");
        const auto loops = CallSites(shader, "write_bda_bytes");
        AgcDriver::Graphics::Require(loops == 1u && lookups == (bits == 32u ? 2u : 1u), "a runtime V# write of " + std::to_string(bits) + " bits has " + std::to_string(lookups) + " write lookups and " + std::to_string(loops) + " byte-loop calls, not its byte path through write_bda_bytes");
    }
}
