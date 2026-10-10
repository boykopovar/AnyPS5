#include <Testing/Test.hpp>
#include "BdaShader.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "SpirvBackend/SpirvBda.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::Require;

constexpr std::array<std::uint32_t, 3> Capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
constexpr std::array<std::string_view, 2> Extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};

template<typename TAction>
void Reject(TAction action, const char* expected) {
    const auto error = Testing::RequireThrows<std::runtime_error>(action, std::string("missing contract rejection: ") + expected);
    Require(std::string(error.what()).find(expected) != std::string::npos, std::string("unexpected contract error: ") + error.what() + ", expected '" + expected + "'");
}

SpirvTargetOptions BdaTarget() {
    return {0x00401000u, 0x00010300u, 32, BdaAbi::Version, Capabilities, Extensions};
}

struct BdaProgram {
    IrProgram program;

    BdaProgram() {
        program.Resources().stage = IrShaderStage::Compute;
        program.Info().usesDma = true;
    }

    void AddBarrier() {
        auto& block = program.CreateBlock();
        block.AppendInstruction(&program.CreateValue(IrOpcode::Barrier, IrType::Void));
        program.BlockOrder().push_back(&block);
    }
};

RecompileRequest BdaRequest() {
    RecompileRequest request{};
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.supportedCapabilities = Capabilities;
    request.target.supportedExtensions = Extensions;
    request.target.fragmentShaderBarycentricEnabled = false;
    return request;
}

const Case completeTarget{"ValidateBdaTarget_CompleteTarget_IsAccepted", [] {
    BdaProgram bda;
    ValidateBdaTarget(bda.program, BdaTarget());
}};

const Case incompleteTarget{"ValidateBdaTarget_MissingAbiVersionCapabilityOrExtension_IsRejected", [] {
    BdaProgram bda;
    auto target = BdaTarget();
    target.bdaAbiVersion = 0;
    Reject([&] { ValidateBdaTarget(bda.program, target); }, "ABI version");
    target = BdaTarget();
    target.supportedCapabilities = {};
    Reject([&] { ValidateBdaTarget(bda.program, target); }, "capability");
    target = BdaTarget();
    target.supportedExtensions = {};
    Reject([&] { ValidateBdaTarget(bda.program, target); }, "extension");
}};

const Case computeBarrier{"ValidateBdaTarget_ComputeProgramWithBarrier_IsAcceptedAndKeepsInvocationsRunning", [] {
    BdaProgram bda;
    bda.AddBarrier();
    ValidateBdaTarget(bda.program, BdaTarget());
    Require(!BdaInvocationsMayStop(bda.program), "a BDA program with barriers must keep its invocations running");
}};

const Case tessellationBarrier{"ValidateBdaTarget_TessellationControlProgramWithBarrier_IsRejected", [] {
    BdaProgram bda;
    bda.AddBarrier();
    bda.program.Resources().stage = IrShaderStage::TessellationControl;
    Reject([&] { ValidateBdaTarget(bda.program, BdaTarget()); }, "barrier-safe");
}};

const Case serializedTarget{"RequestSerializer_BdaTarget_RoundTripsTheContract", [] {
    const RequestSerializer serializer;
    const auto decoded = serializer.Deserialize(serializer.Serialize(BdaRequest()));
    Require(decoded.request.target.bdaAbiVersion == BdaAbi::Version && decoded.request.target.supportedCapabilities.size() == Capabilities.size() && decoded.request.target.supportedExtensions[1] == Extensions[1], "BDA request serialization changed target contract");
}};

const Case changedSignature{"RequestSerializer_ChangedSignature_IsRejected", [] {
    const RequestSerializer serializer;
    auto invalid = serializer.Serialize(BdaRequest());
    invalid[0] = invalid[0] == 'A' ? 'B' : 'A';
    Reject([&] { static_cast<void>(serializer.Deserialize(invalid)); }, "request signature");
}};

const Case oldVersion{"RequestSerializer_OldSerializationVersion_IsRejected", [] {
    const RequestSerializer serializer;
    Reject([&] { static_cast<void>(serializer.Deserialize("NVNQQWMAAAA=")); }, "serialization version");
}};

} // namespace
