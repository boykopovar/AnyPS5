#include "BdaShader.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>
#include <array>
#include <initializer_list>

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

enum ScanId : std::uint32_t {
    IdVoid = 1, IdVoidFunction, IdUint, IdUlong, IdUintPointer, IdAddress, IdOne, IdDeviceScope, IdNoSemantics, IdPointerPointer, IdPointerFunction, IdBool, IdTrue, IdFloat, IdFloatPointer, IdGlsl, IdFunctionUintPointer, IdUntypedPointer, IdFloatOne, IdMain, IdMainLabel, IdPointer, IdHelper, IdHelperParameter, IdHelperLabel, IdBody,
};

bool ScanStores(const std::function<void(ScanWords&)>& main, const std::function<void(ScanWords&)>& after = {}) {
    ScanWords module{{spv::MagicNumber, 0x00010500u, 0u, 200u, 0u}};
    module.Op(spv::OpCapability, {spv::CapabilityShader});
    module.Op(spv::OpCapability, {spv::CapabilityInt64});
    module.Op(spv::OpCapability, {spv::CapabilityPhysicalStorageBufferAddresses});
    module.OpString(spv::OpExtInstImport, {IdGlsl}, "GLSL.std.450");
    module.Op(spv::OpMemoryModel, {spv::AddressingModelPhysicalStorageBuffer64, spv::MemoryModelGLSL450});
    module.OpString(spv::OpEntryPoint, {spv::ExecutionModelGLCompute, IdMain}, "main");
    module.Op(spv::OpTypeVoid, {IdVoid});
    module.Op(spv::OpTypeFunction, {IdVoidFunction, IdVoid});
    module.Op(spv::OpTypeInt, {IdUint, 32u, 0u});
    module.Op(spv::OpTypeInt, {IdUlong, 64u, 0u});
    module.Op(spv::OpTypeFloat, {IdFloat, 32u});
    module.Op(spv::OpTypeBool, {IdBool});
    module.Op(spv::OpTypePointer, {IdUintPointer, spv::StorageClassPhysicalStorageBuffer, IdUint});
    module.Op(spv::OpTypePointer, {IdPointerPointer, spv::StorageClassPhysicalStorageBuffer, IdUintPointer});
    module.Op(spv::OpTypePointer, {IdFloatPointer, spv::StorageClassPhysicalStorageBuffer, IdFloat});
    module.Op(spv::OpTypePointer, {IdPointerFunction, spv::StorageClassFunction, IdUint});
    module.Op(spv::OpTypeUntypedPointerKHR, {IdUntypedPointer, spv::StorageClassPhysicalStorageBuffer});
    module.Op(spv::OpTypeFunction, {IdFunctionUintPointer, IdVoid, IdUintPointer});
    module.Op(spv::OpConstant, {IdUlong, IdAddress, 0x1000u, 0u});
    module.Op(spv::OpConstant, {IdUint, IdOne, 1u});
    module.Op(spv::OpConstant, {IdUint, IdDeviceScope, spv::ScopeDevice});
    module.Op(spv::OpConstant, {IdUint, IdNoSemantics, 0u});
    module.Op(spv::OpConstant, {IdFloat, IdFloatOne, 0x3f800000u});
    module.Op(spv::OpConstantTrue, {IdBool, IdTrue});
    module.Op(spv::OpFunction, {IdVoid, IdMain, spv::FunctionControlMaskNone, IdVoidFunction});
    module.Op(spv::OpLabel, {IdMainLabel});
    module.Op(spv::OpConvertUToPtr, {IdUintPointer, IdPointer, IdAddress});
    main(module);
    module.Op(spv::OpReturn, {});
    module.Op(spv::OpFunctionEnd, {});
    if (after) after(module);
    return AgcDriver::Graphics::SpirvStoresThroughAddresses(module.words);
}

void RunAddressStoreScanTests() {
    using AgcDriver::Graphics::Require;
    Require(!ScanStores([](ScanWords& m) { m.Op(spv::OpLoad, {IdUint, IdBody, IdPointer}); }), "store scan: a load through a physical pointer was taken for a store");
    Require(!ScanStores([](ScanWords& m) { m.Op(spv::OpAtomicLoad, {IdUint, IdBody, IdPointer, IdDeviceScope, IdNoSemantics}); }), "store scan: an atomic load was taken for a store");
    Require(ScanStores([](ScanWords& m) { m.Op(spv::OpStore, {IdPointer, IdOne}); }), "store scan: a store was missed");
    Require(ScanStores([](ScanWords& m) { m.Op(spv::OpAtomicStore, {IdPointer, IdDeviceScope, IdNoSemantics, IdOne}); }), "store scan: an atomic store was missed");
    for (const auto opcode : {spv::OpAtomicExchange, spv::OpAtomicIAdd, spv::OpAtomicISub, spv::OpAtomicSMin, spv::OpAtomicUMin, spv::OpAtomicSMax, spv::OpAtomicUMax, spv::OpAtomicAnd, spv::OpAtomicOr, spv::OpAtomicXor}) {
        Require(ScanStores([opcode](ScanWords& m) { m.Op(opcode, {IdUint, IdBody, IdPointer, IdDeviceScope, IdNoSemantics, IdOne}); }), "store scan: atomic opcode " + std::to_string(static_cast<std::uint32_t>(opcode)) + " was missed");
    }
    Require(ScanStores([](ScanWords& m) { m.Op(spv::OpAtomicIIncrement, {IdUint, IdBody, IdPointer, IdDeviceScope, IdNoSemantics}); }), "store scan: an atomic increment was missed");
    Require(ScanStores([](ScanWords& m) { m.Op(spv::OpAtomicCompareExchange, {IdUint, IdBody, IdPointer, IdDeviceScope, IdNoSemantics, IdNoSemantics, IdOne, IdOne}); }), "store scan: an atomic compare-exchange was missed");
    Require(ScanStores([](ScanWords& m) {
        m.Op(spv::OpConvertUToPtr, {IdPointerPointer, IdBody, IdAddress});
        m.Op(spv::OpLoad, {IdUintPointer, IdBody + 1u, IdBody});
        m.Op(spv::OpStore, {IdBody + 1u, IdOne});
    }), "store scan: a store through a pointer loaded from guest memory was missed");
    Require(ScanStores([](ScanWords& m) {
        m.Op(spv::OpSelect, {IdUintPointer, IdBody, IdTrue, IdPointer, IdPointer});
        m.Op(spv::OpStore, {IdBody, IdOne});
    }), "store scan: a store through a selected pointer was missed");
    Require(ScanStores([](ScanWords& m) { m.Op(spv::OpFunctionCall, {IdVoid, IdBody, IdHelper, IdPointer}); }, [](ScanWords& m) {
        m.Op(spv::OpFunction, {IdVoid, IdHelper, spv::FunctionControlMaskNone, IdFunctionUintPointer});
        m.Op(spv::OpFunctionParameter, {IdUintPointer, IdHelperParameter});
        m.Op(spv::OpLabel, {IdHelperLabel});
        m.Op(spv::OpStore, {IdHelperParameter, IdOne});
        m.Op(spv::OpReturn, {});
        m.Op(spv::OpFunctionEnd, {});
    }), "store scan: a store through a pointer parameter of a called function was missed");
    Require(!ScanStores([](ScanWords& m) {
        m.Op(spv::OpVariable, {IdPointerFunction, IdBody, spv::StorageClassFunction});
        m.Op(spv::OpCopyMemory, {IdBody, IdPointer});
    }), "store scan: a copy out of guest memory was taken for a store");
    Require(ScanStores([](ScanWords& m) {
        m.Op(spv::OpVariable, {IdPointerFunction, IdBody, spv::StorageClassFunction});
        m.Op(spv::OpCopyMemory, {IdPointer, IdBody});
    }), "store scan: a copy into guest memory was missed");
    Require(ScanStores([](ScanWords& m) {
        m.Op(spv::OpConvertUToPtr, {IdUntypedPointer, IdBody, IdAddress});
        m.Op(spv::OpStore, {IdBody, IdOne});
    }), "store scan: a store through an untyped physical pointer was missed");
    Require(ScanStores([](ScanWords& m) {
        m.Op(spv::OpConvertUToPtr, {IdFloatPointer, IdBody, IdAddress});
        m.Op(spv::OpExtInst, {IdFloat, IdBody + 1u, IdGlsl, GLSLstd450Modf, IdFloatOne, IdBody});
    }), "store scan: an extended instruction writing through a physical pointer was missed");
    Require(ScanStores([](ScanWords& m) { m.Op(spv::OpCooperativeMatrixStoreKHR, {IdPointer, IdOne, IdNoSemantics}); }), "store scan: a cooperative matrix store was missed");
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
    using AgcDriver::Graphics::SpirvStoresThroughAddresses;
    RunAddressStoreScanTests();
    AgcDriver::Graphics::Require(!SpirvStoresThroughAddresses(MakeBdaTestShader(0x1000, 32)), "a BDA read was taken for a store through an address");
    AgcDriver::Graphics::Require(!SpirvStoresThroughAddresses(MakeBdaDwordReadTestShader(0x1000, 4, false, false)), "BDA dword reads were taken for a store through an address");
    AgcDriver::Graphics::Require(SpirvStoresThroughAddresses(MakeBdaStoreTestShader(0x1000, 7, 32, false)), "a BDA store was not seen");
    AgcDriver::Graphics::Require(SpirvStoresThroughAddresses(MakeBdaWriteTestShader(0x1000, 7, 32, false)), "a BDA write was not seen");
    const std::array<std::uint32_t, 2> stored{7u, 9u};
    AgcDriver::Graphics::Require(SpirvStoresThroughAddresses(MakeBdaDwordWriteTestShader(0x1000, static_cast<std::uint32_t>(stored.size()), stored.data())), "BDA dword writes were not seen");
    const std::array<std::uint32_t, 5> headerOnly{spv::MagicNumber, 0x00010300u, 0u, 1u, 0u};
    AgcDriver::Graphics::Require(!SpirvStoresThroughAddresses(headerOnly), "a module without physical pointers was taken for one storing through addresses");
    const std::array<std::uint32_t, 2> truncated{spv::MagicNumber, 0u};
    AgcDriver::Graphics::Require(SpirvStoresThroughAddresses(truncated), "an unreadable module was taken for one without stores");
    auto unparsed = MakeBdaStoreTestShader(0x1000, 7, 32, false);
    unparsed[5] |= 0xffff0000u;
    AgcDriver::Graphics::Require(SpirvStoresThroughAddresses(unparsed), "a module with an instruction past its end was taken for one without stores");
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
