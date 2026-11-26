#include "prx/libSceAgcDriver/Execution/include/SubgroupBarrier.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ResourceProgram.hpp"
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
#include <vector>

namespace {

struct Case {
    VkDriverId driver;
    bool controlBarrier;
    bool workgroupFence;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_AMD_PROPRIETARY, false, false},
    {VK_DRIVER_ID_AMD_OPEN_SOURCE, true, true},
    {VK_DRIVER_ID_MESA_RADV, true, true},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, true, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, true, true},
    {VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, true, true},
    {VK_DRIVER_ID_MESA_LLVMPIPE, true, true},
    {VK_DRIVER_ID_MOLTENVK, false, true},
};

constexpr std::array<std::uint32_t, 19> BranchedLdsStep{
    0x34040082u, 0x4a020081u, 0xd8340000u, 0x00000102u, 0xbf8cc07fu, 0xbf8a0000u, 0x7da800ffu, 0x00000080u,
    0xbf880006u, 0xd8d80200u, 0x03000002u, 0xbf8cc07fu, 0x4a020701u, 0xd8340000u, 0x00000102u, 0xbefe03c1u,
    0xbf8cc07fu, 0xbf8a0000u, 0xbf810000u,
};
constexpr std::array<std::uint32_t, 5> EarlyReturnBarrier{0x7d880090u, 0xbf860001u, 0xbf810000u, 0xbf8a0000u, 0xbf810000u};

constexpr std::array<std::uint32_t, 5> LdsHalfStore{0x34040082u, 0xd87c0400u, 0x00000102u, 0xbf8cc07fu, 0xbf810000u};
constexpr std::array<std::uint32_t, 5> LdsAtomicAdd{0x34040082u, 0xd8000400u, 0x00000102u, 0xbf8cc07fu, 0xbf810000u};
constexpr std::array<std::uint32_t, 5> LdsAtomicMax{0x34040082u, 0xd84c0400u, 0x00000102u, 0xbf8cc07fu, 0xbf810000u};
constexpr std::array<std::uint32_t, 8> BufferAtomicAdd{0x34040082u, 0x7e020281u, 0xe0c85000u, 0x80000102u, 0xbf8c3f70u, 0xe0701000u, 0x80000102u, 0xbf810000u};
constexpr std::array<std::uint32_t, 8> BufferAtomicFMin{0x34040082u, 0x7e020300u, 0xe0fc5000u, 0x80000102u, 0xbf8c3f70u, 0xe0701000u, 0x80000102u, 0xbf810000u};

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

ShaderRecompiler::RecompileResult recompileProgram(std::span<const std::uint32_t> code, std::uint32_t threads, bool waveLdsMemoryBarriers, bool deviceScopeLdsMemoryBarriers, bool deviceAtomicWorkgroupMemoryBarriers = false, std::span<const std::uint32_t> userData = {}) {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 3> capabilities{spv::CapabilityShader, spv::CapabilityInt64, spv::CapabilityGroupNonUniform};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 32;
    request.context.userData = userData;
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
    request.target.deviceAtomicWorkgroupMemoryBarriers = deviceAtomicWorkgroupMemoryBarriers;
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
        require(AgcDriver::SubgroupControlBarrierUsable(item.driver) == item.controlBarrier, "driver " + std::to_string(static_cast<int>(item.driver)) + ": expected subgroup control barriers to be " + (item.controlBarrier ? "usable" : "unusable"));
        require(AgcDriver::WorkgroupScopeFenceUsable(item.driver) == item.workgroupFence, "driver " + std::to_string(static_cast<int>(item.driver)) + ": expected workgroup-scope fences to be " + (item.workgroupFence ? "usable" : "unusable"));
    }
}

void verifyMeshSoftwareBarrierDriver() {
    const auto affected = VK_MAKE_API_VERSION(0, 2, 0, 331);
    require(AgcDriver::MeshSoftwareControlBarriersRequired(VK_DRIVER_ID_AMD_PROPRIETARY, affected), "AMD proprietary Vulkan 2.0.331 did not enable mesh software control barriers");
    require(!AgcDriver::MeshSoftwareControlBarriersRequired(VK_DRIVER_ID_AMD_PROPRIETARY, affected - 1u), "the AMD mesh barrier workaround matched the preceding driver version");
    require(!AgcDriver::MeshSoftwareControlBarriersRequired(VK_DRIVER_ID_AMD_PROPRIETARY, affected + 1u), "the AMD mesh barrier workaround matched the following driver version");
    require(!AgcDriver::MeshSoftwareControlBarriersRequired(VK_DRIVER_ID_MESA_RADV, affected), "the AMD mesh barrier workaround matched RADV");
    require(!AgcDriver::MeshSoftwareControlBarriersRequired(VK_DRIVER_ID_NVIDIA_PROPRIETARY, affected), "the AMD mesh barrier workaround matched NVIDIA");
}

struct MeshBarrierOps {
    std::uint32_t control = 0;
    std::uint32_t atomicLoad = 0;
    std::uint32_t atomicIAdd = 0;
    std::uint32_t atomicStore = 0;
    std::uint32_t loopMerge = 0;
    std::uint32_t subgroupBallot = 0;
    std::uint32_t vectorExtractDynamic = 0;
    std::uint32_t selectionMerge = 0;
    bool groupNonUniformCapability = false;
    bool subgroupBallotCapability = false;
    bool releasePathAcquiresGeneration = false;
    bool softwareSafe = false;
};

bool releaseMergeEntersGenerationWait(std::span<const std::uint32_t> words) {
    struct Instruction {
        spv::Op op;
        std::size_t offset;
    };
    std::vector<Instruction> instructions;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        instructions.push_back({static_cast<spv::Op>(words[at] & 0xffffu), at});
    }
    bool releaseStore = false;
    bool generationIncrement = false;
    for (std::size_t i = 0; i < instructions.size(); ++i) {
        const auto& instruction = instructions[i];
        if (instruction.op == spv::OpAtomicStore) {
            releaseStore = true;
            continue;
        }
        if (releaseStore && instruction.op == spv::OpAtomicIAdd) {
            generationIncrement = true;
            continue;
        }
        if (!generationIncrement || instruction.op != spv::OpBranch) continue;
        const auto releaseMerge = words[instruction.offset + 1u];
        if (i + 2u >= instructions.size() || instructions[i + 1u].op != spv::OpLabel ||
            words[instructions[i + 1u].offset + 1u] != releaseMerge || instructions[i + 2u].op != spv::OpBranch) {
            return false;
        }
        const auto waitHeader = words[instructions[i + 2u].offset + 1u];
        for (std::size_t j = i + 3u; j < instructions.size(); ++j) {
            if (instructions[j].op != spv::OpLabel || words[instructions[j].offset + 1u] != waitHeader) continue;
            for (std::size_t k = j + 1u; k < instructions.size() && instructions[k].op != spv::OpLabel; ++k) {
                if (instructions[k].op == spv::OpLoopMerge) return true;
            }
            return false;
        }
        return false;
    }
    return false;
}

MeshBarrierOps recompileMeshBarrier(std::span<const std::uint32_t> code, std::uint64_t codeAddress, bool software, std::uint32_t guestWaveSize = 64u, std::uint32_t hostSubgroupSize = 64u) {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 8> hidden{};
    static constexpr std::array<std::uint32_t, 9> capabilities{
        spv::CapabilityShader, spv::CapabilityImageGatherExtended, spv::CapabilityInt64,
        spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess,
        spv::CapabilityClipDistance, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot,
        spv::CapabilityMeshShadingEXT
    };
    static constexpr std::array<std::string_view, 3> extensions{
        "SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage", "SPV_EXT_mesh_shader"
    };
    RecompileRequest request{};
    request.shader = {ShaderStage::Mesh, codeAddress, code, 0, {}};
    request.context.waveSize = guestWaveSize;
    request.context.userData = hidden;
    request.context.vertex = ShaderVertexStageInfo{};
    request.context.vertex->paClVsOutCntl = (1u << 18u) | (1u << 21u);
    request.graphics = GraphicsCompileContext{0u, {}, MeshConfiguration{4u, 4u, 12u, 12u, 4u, 128u, 1024u, 0u, 4u}, std::nullopt, {}};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010400u;
    request.target.subgroupSize = hostSubgroupSize;
    request.target.bdaAbiVersion = 1u;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024u;
    request.target.maxWorkgroupSharedMemoryBytes = 32768u;
    request.target.mesh = MeshTargetLimits{{128u, 1u, 1u}, 128u, 32768u, 256u, 256u, 128u, 32768u, 1u, 1u};
    request.target.meshSoftwareControlBarriers = software;
    request.layout = {0u, 0u, 0u, 128u};
    request.useCache = true;
    const auto program = PrepareResourceProgram(request);
    const auto result = Recompile(request);
    MeshBarrierOps ops;
    ops.softwareSafe = program.Metadata().meshSoftwareControlBarriersSafe;
    const auto& words = result.spirv.Words();
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        switch (static_cast<spv::Op>(words[at] & 0xffffu)) {
        case spv::OpControlBarrier: ++ops.control; break;
        case spv::OpAtomicLoad: ++ops.atomicLoad; break;
        case spv::OpAtomicIAdd: ++ops.atomicIAdd; break;
        case spv::OpAtomicStore: ++ops.atomicStore; break;
        case spv::OpLoopMerge: ++ops.loopMerge; break;
        case spv::OpGroupNonUniformBallot: ++ops.subgroupBallot; break;
        case spv::OpVectorExtractDynamic: ++ops.vectorExtractDynamic; break;
        case spv::OpCapability:
            if (words[at + 1u] == spv::CapabilityGroupNonUniform) ops.groupNonUniformCapability = true;
            if (words[at + 1u] == spv::CapabilityGroupNonUniformBallot) ops.subgroupBallotCapability = true;
            break;
        case spv::OpSelectionMerge: ++ops.selectionMerge; break;
        default: break;
        }
    }
    ops.releasePathAcquiresGeneration = releaseMergeEntersGenerationWait(words);
    return ops;
}

void verifyMeshSoftwareBarrierEmission() {
    const auto normal = recompileMeshBarrier(BranchedLdsStep, 0x26267000u, false);
    require(normal.softwareSafe, "the ordinary mesh barrier fixture was not marked safe for software rendezvous");
    require(normal.control >= 2u, "the mesh barrier fixture did not emit both its guest and wrapper control barriers");
    require(normal.atomicIAdd == 0u && normal.atomicLoad == 0u && normal.atomicStore == 0u, "the normal mesh barrier fixture unexpectedly used software barrier atomics");
    const auto software = recompileMeshBarrier(BranchedLdsStep, 0x26267000u, true);
    require(software.control == 2u, "the software mesh barrier path did not retain only the two unconditional wrapper control barriers");
    require(software.atomicLoad >= 2u && software.atomicIAdd >= 2u && software.atomicStore >= 1u && software.loopMerge >= 1u, "the software mesh barrier path did not emit the cyclic atomic rendezvous");
    require(software.subgroupBallot >= normal.subgroupBallot + 2u, "the software mesh barrier path did not make its release and wait decisions uniform across the host subgroup");
    require(software.groupNonUniformCapability && software.subgroupBallotCapability, "the software mesh barrier path emitted subgroup ballots without declaring the required SPIR-V capabilities");
    require(software.releasePathAcquiresGeneration, "invocations in the releasing subgroup bypassed the generation-acquire loop");
    require(software.selectionMerge >= normal.selectionMerge + 2u, "the software mesh barrier path did not reconverge the single-lane release before leaving its subgroup");
    const auto splitNormal = recompileMeshBarrier(BranchedLdsStep, 0x26267200u, false, 32u, 64u);
    const auto splitSoftware = recompileMeshBarrier(BranchedLdsStep, 0x26267200u, true, 32u, 64u);
    require(splitSoftware.subgroupBallot >= splitNormal.subgroupBallot + 2u, "the split-subgroup barrier did not emit both host-subgroup votes");
    require(splitSoftware.vectorExtractDynamic == splitNormal.vectorExtractDynamic, "the software mesh barrier narrowed its rendezvous votes to one guest-wave half of a 64-lane host subgroup");
    require(splitSoftware.subgroupBallotCapability, "the split-subgroup mesh software barrier omitted the subgroup-ballot capability");
    const auto singleLane = recompileMeshBarrier(BranchedLdsStep, 0x26267300u, true, 32u, 1u);
    require(singleLane.subgroupBallot == 0u && !singleLane.groupNonUniformCapability && !singleLane.subgroupBallotCapability, "a single-lane host subgroup emitted an unnecessary subgroup operation or capability");
    const auto earlyReturn = recompileMeshBarrier(EarlyReturnBarrier, 0x26267100u, true);
    require(!earlyReturn.softwareSafe, "a mesh barrier bypassed by an early return was marked safe for software rendezvous");
    require(earlyReturn.atomicLoad == 0u && earlyReturn.atomicIAdd == 0u && earlyReturn.atomicStore == 0u, "a mesh barrier bypassed by an early return used a software rendezvous that can wait forever");
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

void verifyLdsLockFenceDrivers() {
    for (const auto& item : Cases) {
        require(AgcDriver::LdsLockFencesRequired(item.driver) == (item.driver == VK_DRIVER_ID_MOLTENVK), "driver " + std::to_string(static_cast<int>(item.driver)) + ": expected LDS lock fences only on MoltenVK");
        require(AgcDriver::DeviceAtomicWorkgroupMemoryBarriersRequired(item.driver) == (item.driver == VK_DRIVER_ID_AMD_PROPRIETARY), "driver " + std::to_string(static_cast<int>(item.driver)) + ": expected workgroup-memory device atomic barriers only on AMD proprietary");
    }
}

void verifyDeviceAtomicMemoryBarrierSemantics() {
    const std::array<std::uint32_t, 4> descriptor{0x1000u, 0u, 256u, 0x31016facu};
    const std::array<std::pair<const char*, std::span<const std::uint32_t>>, 2> programs{{{"buffer_atomic_add", BufferAtomicAdd}, {"buffer_atomic_fmin", BufferAtomicFMin}}};
    const auto inspect = [&](std::span<const std::uint32_t> code, bool workgroupSemantics) {
        const auto result = recompileProgram(code, 64, false, false, workgroupSemantics, descriptor);
        const auto& words = result.spirv.Words();
        const auto constants = constantsOf(words);
        std::uint32_t uniformCount = 0;
        std::uint32_t workgroupCount = 0;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
            if (static_cast<spv::Op>(words[at] & 0xffffu) != spv::OpMemoryBarrier) continue;
            const auto scope = constants.find(words[at + 1]);
            const auto semantics = constants.find(words[at + 2]);
            require(scope != constants.end() && semantics != constants.end(), "device atomic barrier operands are not 32-bit constants");
            require(scope->second == spv::ScopeDevice, "device atomic barrier did not use device scope");
            if (semantics->second == (spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsUniformMemoryMask)) ++uniformCount;
            if (semantics->second == (spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask)) ++workgroupCount;
        }
        return std::pair{uniformCount, workgroupCount};
    };
    for (const auto& [name, code] : programs) {
        const auto uniform = inspect(code, false);
        require(uniform.first == 1 && uniform.second == 0, std::string(name) + " did not emit UniformMemory barrier by default");
        const auto workgroup = inspect(code, true);
        require(workgroup.first == 0 && workgroup.second == 1, std::string(name) + " with workgroup-memory barrier flag did not emit WorkgroupMemory barrier");
    }
}

}

int main() {
    try {
        verifyDriverQuirk();
        verifyMeshSoftwareBarrierDriver();
        verifyMeshSoftwareBarrierEmission();
        verifyLdsLockFenceDrivers();
        verifyEmittedBarriers();
        verifyLdsMemoryBarrierScope();
        verifyDeviceAtomicMemoryBarrierSemantics();
        std::puts("subgroup barrier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
