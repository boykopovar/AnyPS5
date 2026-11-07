#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

RecompileRequest ComputeRequest(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData) {
    static const std::array<std::uint32_t, 2> capabilities{1u, 61u};
    static const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 64u;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 64u;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    return request;
}

const IrValue* Operand(const IrValue* value, std::size_t index) {
    require(value != nullptr && index < value->ArgumentCount(), "a resolved value lacks an operand");
    const auto* operand = value->Argument(index);
    require(operand != nullptr, "a resolved value has a null operand");
    return operand->Resolve();
}

std::uint32_t UserDataRegister(const IrValue* value) {
    require(!value->IsPhi(), "the T# address still merges values from both loop exits");
    require(value->Opcode() == IrOpcode::GetUserData, "the T# address is not the user data the loop sets before its early exit");
    const auto* reg = Operand(value, 0u);
    require(reg->Type() == IrType::ScalarReg, "user data without a scalar register operand");
    return reg->Register().index;
}

void verifyDescriptorBelowGotoBranch() {
    const std::vector<std::uint32_t> code{
        0x7e020280u, 0xbe880380u,
        0xbe900304u, 0xbe910305u, 0xbf060608u, 0xbf850009u,
        0xf4240400u, 0xfa000010u, 0xbf8cc07fu,
        0x80088108u, 0xbf0a8408u, 0xbf85fff6u,
        0xe0700000u, 0x80000100u, 0xbf810000u,
        0xf40c0608u, 0xfa000000u, 0xbf8cc07fu,
        0xf0001108u, 0x00060300u, 0xbf8c3f70u,
        0xe0700000u, 0x80000300u, 0xbf810000u};
    const std::array<std::uint32_t, 8> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu, 0x20000000u, 0x0u, 3u, 0u};
    auto program = PrepareResourceProgram(ComputeRequest(code, userData));
    static_cast<void>(ResourceMaterializer{}.ExtractPlan(program));
    const auto& plan = program.Resources();
    require(plan.info.images.size() == 1u, "the image loaded below the goto branch was not planned");
    const auto& source = plan.descriptorSources.at(plan.info.images[0].source);
    require(source.dwordCount == 8u && !source.tableColumn.has_value(), "the T# is not an eight-dword descriptor read through the SRT");
    for (std::uint32_t dword = 0; dword < source.dwordCount; ++dword) {
        const auto* read = source.dwords[dword] != nullptr ? source.dwords[dword]->Resolve() : nullptr;
        require(read != nullptr && read->Opcode() == IrOpcode::ReadConst, "T# dword " + std::to_string(dword) + " is not read through the SRT");
        require(Operand(read, 0u)->Opcode() == IrOpcode::GetSrtResource && Operand(read, 1u)->HasImmediate(), "T# dword " + std::to_string(dword) + " does not name an SRT slot");
        const auto slot = Operand(read, 1u)->ImmediateU32();
        require(slot < plan.srtReads.size() && plan.srtReads[slot].value != nullptr, "T# dword " + std::to_string(dword) + " names a missing SRT slot");
        const auto* load = plan.srtReads[slot].value->Resolve();
        require(load->Opcode() == IrOpcode::LoadAddressU32, "SRT slot " + std::to_string(slot) + " is not a load through an address");
        const auto* address = Operand(load, 0u);
        require(address->Opcode() == IrOpcode::GetAddressResource, "SRT slot " + std::to_string(slot) + " is not loaded through an address resource");
        require(UserDataRegister(Operand(address, 0u)) == 4u && UserDataRegister(Operand(address, 1u)) == 5u, "SRT slot " + std::to_string(slot) + " is not loaded through s[4:5]");
    }
}

}

int main() {
    try {
        verifyDescriptorBelowGotoBranch();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "goto junction descriptor: %s\n", error.what());
        return 1;
    }
    std::puts("Goto junction descriptor tests passed");
    return 0;
}
