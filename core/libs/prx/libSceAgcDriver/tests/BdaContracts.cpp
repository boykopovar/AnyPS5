#include "BdaShader.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace {

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
    for (const std::uint32_t bits : {8u, 16u, 32u}) {
        const auto shader = MakeBdaWriteTestShader(0x7fff1235fffdULL, 0xa1b2c3d4u, bits, false);
        const auto lookups = CallSites(shader, "get_bda_write_pointer");
        const auto loops = CallSites(shader, "write_bda_bytes");
        AgcDriver::Graphics::Require(loops == 1u && lookups == (bits == 32u ? 2u : 1u), "a runtime V# write of " + std::to_string(bits) + " bits has " + std::to_string(lookups) + " write lookups and " + std::to_string(loops) + " byte-loop calls, not its byte path through write_bda_bytes");
    }
}
