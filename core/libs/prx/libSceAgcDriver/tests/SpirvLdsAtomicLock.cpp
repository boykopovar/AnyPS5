#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

constexpr std::uint32_t DsAddU64 = 0xd9000000u;
constexpr std::uint32_t DsAddU32Hi = 0xd8000004u;
constexpr std::uint32_t DsIncU32Hi = 0xd80c0004u;
constexpr std::uint32_t DsOperands = 0x00000201u;
constexpr std::uint32_t DsOperand32 = 0x00000401u;
constexpr std::uint32_t FlatAtomicAddU32 = 0xdcc90000u;
constexpr std::uint32_t FlatAtomicOperands = 0x0a7d0402u;

struct Usage {
    std::size_t elects = 0;
    std::size_t exchanges = 0;
    std::size_t releases = 0;
    std::size_t storeReleases = 0;
    std::size_t adds = 0;
    bool addBetweenLock = false;
    bool lockInitIsLaneZero = false;
    bool fenceBetweenLock = false;
    bool releaseWithoutReleaseSemantics = false;
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint32_t> RecompileProgram(std::span<const std::uint32_t> code) {
    const std::array<std::uint32_t, 7> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::uint32_t, 4> userData{};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 32;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 4u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 32;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    auto result = Recompile(request);
    Require(!result.spirv.empty(), "the LDS atomic program did not recompile");
    return std::vector<std::uint32_t>(result.spirv.Words().begin(), result.spirv.Words().end());
}

Usage Scan(std::span<const std::uint32_t> words) {
    Usage usage;
    bool locked = false;
    std::uint32_t localIndexVariable = 0u;
    std::uint32_t localIndexLoad = 0u;
    std::uint32_t u32Type = 0u;
    std::uint32_t zero = 0u;
    std::uint32_t laneZeroCondition = 0u;
    std::uint32_t laneZeroBody = 0u;
    std::uint32_t lockPointer = 0u;
    std::map<std::uint32_t, std::uint32_t> constants;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> spv::WordCountShift;
        const auto op = static_cast<spv::Op>(words[cursor] & spv::OpCodeMask);
        Require(count != 0 && cursor + count <= words.size(), "truncated SPIR-V instruction");
        if (op == spv::OpDecorate && count >= 4u && words[cursor + 2u] == spv::DecorationBuiltIn && words[cursor + 3u] == spv::BuiltInLocalInvocationIndex) {
            localIndexVariable = words[cursor + 1u];
        }
        if (op == spv::OpTypeInt && count >= 4u && words[cursor + 2u] == 32u && words[cursor + 3u] == 0u) u32Type = words[cursor + 1u];
        if (op == spv::OpConstant && count >= 4u && words[cursor + 1u] == u32Type && words[cursor + 3u] == 0u) zero = words[cursor + 2u];
        if (op == spv::OpLoad && count >= 4u && words[cursor + 3u] == localIndexVariable) localIndexLoad = words[cursor + 2u];
        if (op == spv::OpIEqual && count >= 5u && ((words[cursor + 3u] == localIndexLoad && words[cursor + 4u] == zero) || (words[cursor + 4u] == localIndexLoad && words[cursor + 3u] == zero))) {
            laneZeroCondition = words[cursor + 2u];
        }
        if (op == spv::OpBranchConditional && count >= 4u && words[cursor + 1u] == laneZeroCondition) laneZeroBody = words[cursor + 2u];
        if (op == spv::OpConstant && count == 4) constants[words[cursor + 2]] = words[cursor + 3];
        if (op == spv::OpGroupNonUniformElect) ++usage.elects;
        if (op == spv::OpAtomicCompareExchange) {
            ++usage.exchanges;
            locked = true;
            if (count >= 4u) lockPointer = words[cursor + 3u];
        }
        if (op == spv::OpAtomicExchange) {
            ++usage.releases;
            const auto semantics = constants.find(words[cursor + 5]);
            if (semantics == constants.end() || (semantics->second & spv::MemorySemanticsReleaseMask) == 0) usage.releaseWithoutReleaseSemantics = true;
            locked = false;
        }
        if (op == spv::OpAtomicStore) {
            ++usage.storeReleases;
            locked = false;
        }
        if (op == spv::OpMemoryBarrier && locked) usage.fenceBetweenLock = true;
        if (op == spv::OpAtomicIAdd) {
            ++usage.adds;
            if (locked) usage.addBetweenLock = true;
        }
        cursor += count;
    }
    std::uint32_t lockBase = 0u;
    std::uint32_t lockIndex = 0u;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> spv::WordCountShift;
        const auto op = static_cast<spv::Op>(words[cursor] & spv::OpCodeMask);
        if (op == spv::OpAccessChain && count >= 5u && words[cursor + 2u] == lockPointer) {
            lockBase = words[cursor + 3u];
            lockIndex = words[cursor + 4u];
            break;
        }
        cursor += count;
    }
    std::vector<std::uint32_t> lockPointers{lockPointer};
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> spv::WordCountShift;
        const auto op = static_cast<spv::Op>(words[cursor] & spv::OpCodeMask);
        if (op == spv::OpAccessChain && count >= 5u && words[cursor + 3u] == lockBase && words[cursor + 4u] == lockIndex) {
            lockPointers.push_back(words[cursor + 2u]);
        }
        cursor += count;
    }
    bool inLaneZeroBody = false;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> spv::WordCountShift;
        const auto op = static_cast<spv::Op>(words[cursor] & spv::OpCodeMask);
        if (op == spv::OpLabel) inLaneZeroBody = words[cursor + 1u] == laneZeroBody;
        if (inLaneZeroBody && op == spv::OpStore && count >= 3u && std::find(lockPointers.begin(), lockPointers.end(), words[cursor + 1u]) != lockPointers.end()) usage.lockInitIsLaneZero = true;
        if (inLaneZeroBody && (op == spv::OpBranch || op == spv::OpBranchConditional || op == spv::OpReturn)) inLaneZeroBody = false;
        cursor += count;
    }
    return usage;
}

Usage Compile(std::initializer_list<std::uint32_t> body) {
    std::vector<std::uint32_t> code{0x7e020280u, 0x7e0402c1u, 0x7e060280u, 0x7e080281u};
    code.insert(code.end(), body);
    code.insert(code.end(), {0xbf8cc07fu, 0xbf810000u});
    return Scan(RecompileProgram(code));
}

void Check64BitOnlyLocks() {
    const auto usage = Compile({DsAddU64, DsOperands});
    Require(usage.elects == 1 && usage.exchanges == 1 && usage.releases == 1, "a ds_add_u64 is not serialised by one elected lane taking the workgroup lock");
    Require(usage.lockInitIsLaneZero, "the workgroup lock is not initialized by only local invocation zero");
    Require(usage.storeReleases == 0 && !usage.releaseWithoutReleaseSemantics, "the workgroup lock is not released by an exchange with release semantics: atomic stores " + std::to_string(usage.storeReleases));
    Require(!usage.fenceBetweenLock, "a ds_add_u64 emits a memory barrier while it holds the workgroup lock");
}

void Check32BitTakesLockWith64Bit() {
    const auto usage = Compile({DsAddU64, DsOperands, DsAddU32Hi, DsOperand32});
    Require(usage.elects == 2 && usage.exchanges == 2 && usage.releases == 2, "ds_add_u32 in a program with ds_add_u64 does not take the workgroup lock: elect " + std::to_string(usage.elects) + ", lock acquire " + std::to_string(usage.exchanges) + ", lock release " + std::to_string(usage.releases) + ", expected 2 each");
    Require(usage.adds == 1 && usage.addBetweenLock, "the 32-bit OpAtomicIAdd is not between the lock acquire and the lock release");
    Require(!usage.fenceBetweenLock, "ds_add_u32 emits a memory barrier while it holds the workgroup lock");
}

void CheckOtherUpdatePathTakesLock() {
    const auto usage = Compile({DsAddU64, DsOperands, DsIncU32Hi, DsOperand32});
    Require(usage.elects == 2 && usage.exchanges >= 2 && usage.releases == 2, "ds_inc_u32 in a program with ds_add_u64 does not take the workgroup lock: elect " + std::to_string(usage.elects) + ", lock release " + std::to_string(usage.releases) + ", expected 2 each");
    Require(!usage.fenceBetweenLock, "ds_inc_u32 emits a memory barrier while it holds the workgroup lock");
}

void CheckFlatAtomicTakesLockWith64Bit() {
    const auto usage = Compile({DsAddU64, DsOperands, FlatAtomicAddU32, FlatAtomicOperands});
    Require(usage.elects == 2, "a 32-bit flat atomic into the LDS aperture in a program with ds_add_u64 does not take the workgroup lock: elect " + std::to_string(usage.elects) + ", expected 2");
    Require(!usage.fenceBetweenLock, "a 32-bit flat atomic into the LDS aperture emits a memory barrier while it holds the workgroup lock");
}

void CheckFlatAtomicOnlyIsUnchanged() {
    const auto usage = Compile({FlatAtomicAddU32, FlatAtomicOperands});
    Require(usage.elects == 0, "a program with only a 32-bit flat atomic got a workgroup lock: elect " + std::to_string(usage.elects));
}

void Check32BitOnlyIsUnchanged() {
    const auto add = Compile({DsAddU32Hi, DsOperand32});
    Require(add.elects == 0 && add.exchanges == 0 && add.releases == 0 && add.adds == 1, "a program with only ds_add_u32 got a workgroup lock: elect " + std::to_string(add.elects) + ", lock acquire " + std::to_string(add.exchanges) + ", lock release " + std::to_string(add.releases) + ", native add " + std::to_string(add.adds));
    const auto inc = Compile({DsIncU32Hi, DsOperand32});
    Require(inc.elects == 0 && inc.releases == 0, "a program with only ds_inc_u32 got a workgroup lock: elect " + std::to_string(inc.elects) + ", lock release " + std::to_string(inc.releases));
}

}

int main() {
    try {
        Check64BitOnlyLocks();
        Check32BitTakesLockWith64Bit();
        CheckOtherUpdatePathTakesLock();
        CheckFlatAtomicTakesLockWith64Bit();
        CheckFlatAtomicOnlyIsUnchanged();
        Check32BitOnlyIsUnchanged();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SpirvLdsAtomicLock: %s\n", error.what());
        return 1;
    }
    std::puts("SpirvLdsAtomicLock: 32-bit LDS atomics take the 64-bit workgroup lock only in programs with a 64-bit LDS atomic");
    return 0;
}
