#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <set>
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

std::uint64_t workgroupBytes(std::span<const std::uint32_t> words) {
    std::map<std::uint32_t, std::span<const std::uint32_t>> types;
    std::map<std::uint32_t, std::uint32_t> constants;
    std::vector<std::uint32_t> pointers;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> 16u;
        require(count != 0u && count <= words.size() - offset, "malformed SPIR-V instruction");
        const auto instruction = words.subspan(offset, count);
        const auto opcode = static_cast<spv::Op>(instruction[0] & 0xffffu);
        if (opcode == spv::OpTypeInt || opcode == spv::OpTypeFloat || opcode == spv::OpTypeVector || opcode == spv::OpTypeArray || opcode == spv::OpTypeStruct || opcode == spv::OpTypePointer) types.emplace(instruction[1], instruction);
        if (opcode == spv::OpConstant) constants.emplace(instruction[2], instruction[3]);
        if (opcode == spv::OpVariable && instruction[3] == spv::StorageClassWorkgroup) pointers.push_back(instruction[1]);
        offset += count;
    }
    const std::function<std::uint64_t(std::uint32_t)> size = [&](std::uint32_t id) -> std::uint64_t {
        const auto type = types.at(id);
        switch (static_cast<spv::Op>(type[0] & 0xffffu)) {
        case spv::OpTypeInt: case spv::OpTypeFloat: return type[2] / 8u;
        case spv::OpTypeVector: return size(type[2]) * type[3];
        case spv::OpTypeArray: return size(type[2]) * constants.at(type[3]);
        case spv::OpTypeStruct: {
            std::uint64_t bytes = 0;
            for (const auto member : type.subspan(2)) bytes += size(member);
            return bytes;
        }
        default: throw std::runtime_error("unexpected Workgroup variable type");
        }
    };
    std::uint64_t bytes = 0;
    for (const auto pointer : pointers) {
        const auto type = types.at(pointer);
        require((type[0] & 0xffffu) == spv::OpTypePointer, "Workgroup variable is not a pointer");
        bytes += size(type[3]);
    }
    return bytes;
}

std::set<std::uint32_t> compareExchangeWidths(std::span<const std::uint32_t> words) {
    std::map<std::uint32_t, std::uint32_t> widths;
    std::set<std::uint32_t> found;
    std::uint32_t faultFunction = 0;
    bool inFaultFunction = false;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> 16u;
        require(count != 0u && count <= words.size() - offset, "malformed SPIR-V instruction");
        const auto instruction = words.subspan(offset, count);
        const auto opcode = static_cast<spv::Op>(instruction[0] & 0xffffu);
        if (opcode == spv::OpName && std::string_view(reinterpret_cast<const char*>(instruction.data() + 2)) == "record_bda_fault") faultFunction = instruction[1];
        if (opcode == spv::OpFunction) inFaultFunction = instruction[2] == faultFunction;
        if (opcode == spv::OpTypeInt) widths.emplace(instruction[1], instruction[2]);
        if (opcode == spv::OpAtomicCompareExchange && !inFaultFunction) found.insert(widths.at(instruction[1]));
        offset += count;
    }
    return found;
}

RecompileResult compile(std::uint32_t waveSize, std::uint32_t limit, bool int64Atomics) {
    static constexpr std::array<std::uint32_t, 8> code{
        0x7e020280u, 0x7e0402c1u, 0x7e060280u, 0x7e080281u,
        0xd9000000u, 0x00000201u, 0xbf8cc07fu, 0xbf810000u
    };
    static constexpr std::array<std::uint32_t, 8> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityInt64Atomics};
    static constexpr std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    static constexpr std::array<std::uint32_t, 4> userData{};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = waveSize;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 16u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 32;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = std::span(capabilities).first(int64Atomics ? 8u : 7u);
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = limit;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    return Recompile(request);
}

void verify(std::uint32_t waveSize, std::uint32_t limit, bool int64Atomics) {
    const auto result = compile(waveSize, limit, int64Atomics);
    require(!result.spirv.empty(), "LDS atomic program did not compile");
    const auto declared = workgroupBytes(result.spirv.Words());
    require(declared <= limit, "declared " + std::to_string(declared) + " Workgroup bytes exceeds limit " + std::to_string(limit));
    require(result.workgroupMemoryDwords == (limit < 68u ? 18u : 0u), "incorrect device-memory LDS stride at limit " + std::to_string(limit));
    if (limit >= 68u) require(declared != 0u, "in-budget LDS unexpectedly lost its Workgroup allocation");
    const auto widths = compareExchangeWidths(result.spirv.Words());
    const bool native = int64Atomics && limit < 68u;
    require(widths.contains(64u) == native, native ? "device-memory LDS did not use a 64-bit compare exchange" : "a 64-bit compare exchange appeared without device-memory LDS and shaderBufferInt64Atomics");
    require(widths.contains(32u) != native, native ? "device-memory LDS with shaderBufferInt64Atomics kept the workgroup lock" : "the 64-bit LDS atomic lost its workgroup lock");
    require(result.bdaAbiVersion == (limit < 68u ? 1u : 0u), "the fault buffer for a misaligned 64-bit LDS atomic does not follow device-memory LDS at limit " + std::to_string(limit));
}

}

int main() {
    unsigned failures = 0;
    for (const auto waveSize : {32u, 64u}) {
        for (const auto limit : {64u, 68u, 72u}) {
            for (const bool int64Atomics : {false, true}) {
                const auto name = "wave" + std::to_string(waveSize) + " limit" + std::to_string(limit) + (int64Atomics ? " int64 atomics" : "");
                try {
                    verify(waveSize, limit, int64Atomics);
                    std::cout << "LDS lock budget: " << name << " passed\n";
                } catch (const std::exception& error) {
                    ++failures;
                    const std::string message(error.what());
                    std::cerr << "LDS lock budget: " << name << ": " << message.substr(0, message.find("RecompileRequest:")) << '\n';
                }
            }
        }
    }
    if (failures != 0) return 1;
    std::cout << "LDS lock budget tests passed\n";
    return 0;
}
