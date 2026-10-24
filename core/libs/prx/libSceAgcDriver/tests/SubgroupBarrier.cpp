#include "prx/libSceAgcDriver/Execution/include/SubgroupBarrier.hpp"
#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {

struct Case {
    VkDriverId driver;
    bool usable;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_AMD_PROPRIETARY, false},
    {VK_DRIVER_ID_AMD_OPEN_SOURCE, true},
    {VK_DRIVER_ID_MESA_RADV, true},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, true},
    {VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, true},
    {VK_DRIVER_ID_MESA_LLVMPIPE, true},
    {VK_DRIVER_ID_MOLTENVK, true},
};

constexpr std::array<std::uint32_t, 19> BranchedLdsStep{
    0x34040082u, 0x4a020081u, 0xd8340000u, 0x00000102u, 0xbf8cc07fu, 0xbf8a0000u, 0x7da800ffu, 0x00000080u,
    0xbf880006u, 0xd8d80200u, 0x03000002u, 0xbf8cc07fu, 0x4a020701u, 0xd8340000u, 0x00000102u, 0xbefe03c1u,
    0xbf8cc07fu, 0xbf8a0000u, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 5> LdsHalfStore{0x34040082u, 0xd87c0400u, 0x00000102u, 0xbf8cc07fu, 0xbf810000u};
constexpr std::array<std::uint32_t, 5> LdsAtomicAdd{0x34040082u, 0xd8000400u, 0x00000102u, 0xbf8cc07fu, 0xbf810000u};
constexpr std::array<std::uint32_t, 5> LdsAtomicMax{0x34040082u, 0xd84c0400u, 0x00000102u, 0xbf8cc07fu, 0xbf810000u};

struct Barriers {
    std::uint32_t subgroupControl = 0;
    std::uint32_t subgroupMemory = 0;
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct MemoryBarriers {
    std::uint32_t workgroup = 0;
    std::uint32_t device = 0;
};

ShaderRecompiler::RecompileResult recompileProgram(std::span<const std::uint32_t> code, std::uint32_t threads, bool waveLdsMemoryBarriers, bool deviceScopeLdsMemoryBarriers) {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 2> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 32;
    request.context.compute = ShaderComputeStageInfo{{threads, 1u, 1u}, 512u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 32;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.target.waveLdsMemoryBarriers = waveLdsMemoryBarriers;
    request.target.deviceScopeLdsMemoryBarriers = deviceScopeLdsMemoryBarriers;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    return Recompile(request);
}

std::unordered_map<std::uint32_t, std::uint32_t> constantsOf(std::span<const std::uint32_t> words) {
    std::unordered_map<std::uint32_t, std::uint32_t> constants;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) == spv::OpConstant && (words[at] >> 16u) == 4u) constants[words[at + 2]] = words[at + 3];
    }
    return constants;
}

MemoryBarriers memoryBarriers(std::span<const std::uint32_t> code, bool deviceScopeLdsMemoryBarriers) {
    const auto result = recompileProgram(code, 128, false, deviceScopeLdsMemoryBarriers);
    const auto& words = result.spirv.Words();
    const auto constants = constantsOf(words);
    MemoryBarriers barriers;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) != spv::OpMemoryBarrier) continue;
        const auto scope = constants.find(words[at + 1]);
        require(scope != constants.end(), "a memory barrier scope is not a 32-bit constant");
        if (scope->second == spv::ScopeWorkgroup) ++barriers.workgroup;
        if (scope->second == spv::ScopeDevice) ++barriers.device;
    }
    return barriers;
}

Barriers recompile(std::uint32_t threads, bool waveLdsMemoryBarriers) {
    const auto result = recompileProgram(BranchedLdsStep, threads, waveLdsMemoryBarriers, false);
    const auto& words = result.spirv.Words();
    const auto constants = constantsOf(words);
    Barriers barriers;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op != spv::OpControlBarrier && op != spv::OpMemoryBarrier) continue;
        const auto scope = constants.find(words[at + 1]);
        require(scope != constants.end(), "a barrier scope is not a 32-bit constant");
        if (scope->second != spv::ScopeSubgroup) continue;
        if (op == spv::OpControlBarrier) ++barriers.subgroupControl;
        else ++barriers.subgroupMemory;
    }
    return barriers;
}

void verifyDriverQuirk() {
    for (const auto& item : Cases) {
        require(AgcDriver::SubgroupControlBarrierUsable(item.driver) == item.usable, "driver " + std::to_string(static_cast<int>(item.driver)) + ": expected subgroup control barriers to be " + (item.usable ? "usable" : "unusable"));
        require(AgcDriver::WorkgroupScopeFenceUsable(item.driver) == item.usable, "driver " + std::to_string(static_cast<int>(item.driver)) + ": expected workgroup-scope fences to be " + (item.usable ? "usable" : "unusable"));
    }
}

void verifyLdsMemoryBarrierScope() {
    const std::array<std::pair<const char*, std::span<const std::uint32_t>>, 3> programs{{{"ds_write_b16", LdsHalfStore}, {"ds_add_u32", LdsAtomicAdd}, {"ds_max_f32", LdsAtomicMax}}};
    for (const auto& [name, code] : programs) {
        const auto workgroup = memoryBarriers(code, false);
        require(workgroup.workgroup == 1 && workgroup.device == 0, std::string(name) + " did not order its LDS update with one workgroup-scope OpMemoryBarrier");
        const auto device = memoryBarriers(code, true);
        require(device.workgroup == 0 && device.device == 1, std::string(name) + " with device-scope LDS memory barriers did not order its LDS update with one device-scope OpMemoryBarrier");
    }
}

void verifyEmittedBarriers() {
    const auto control = recompile(256, false);
    require(control.subgroupControl != 0 && control.subgroupMemory == 0, "a wave32 program in a workgroup of eight waves did not order its LDS accesses with subgroup-scope OpControlBarrier only");
    const auto memory = recompile(256, true);
    require(memory.subgroupControl == 0 && memory.subgroupMemory != 0, "with wave LDS memory barriers, a wave32 program in a workgroup of eight waves did not order its LDS accesses with subgroup-scope OpMemoryBarrier only");
    const auto single = recompile(32, true);
    require(single.subgroupControl == 0 && single.subgroupMemory == 0, "with wave LDS memory barriers, a single-wave workgroup did not keep workgroup-scope barriers");
}

}

int main() {
    try {
        verifyDriverQuirk();
        verifyEmittedBarriers();
        verifyLdsMemoryBarrierScope();
        std::puts("subgroup barrier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
