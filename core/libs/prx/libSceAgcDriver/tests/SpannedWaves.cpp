#include "prx/libSceAgcDriver/Execution/include/SpannedWaves.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvEmitterState.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using namespace ShaderRecompiler;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Detection {
    const char* name;
    VkDriverId driver;
    const char* driverInfo;
    std::uint32_t minSubgroupSize;
    bool computeFullSubgroups;
    bool shared;
    bool versionUnknown;
};

constexpr Detection Detections[]{
    {"Intel's Windows driver 101.9150", VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, "Intel Corporation 101.9150", 8u, true, true, false},
    {"Intel's Windows driver without a version", VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, "", 8u, true, true, false},
    {"ANV, Mesa 25.2.7", VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, "Mesa 25.2.7", 8u, true, true, false},
    {"ANV, Mesa 25.2.7 after the driver name", VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, "Intel open-source Mesa driver Mesa 25.2.7", 8u, true, true, false},
    {"ANV, Mesa 24.0.5 from a distribution", VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, "Intel open-source Mesa driver Mesa 24.0.5-1ubuntu1", 8u, true, true, false},
    {"ANV, Mesa 25.3.0", VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, "Mesa 25.3.0", 8u, true, false, false},
    {"ANV, Mesa 26.2.4", VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, "Intel open-source Mesa driver Mesa 26.2.4", 8u, true, false, false},
    {"ANV without a Mesa version", VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, "Intel open-source Mesa driver", 8u, true, true, true},
    {"ANV, Mesa 25.2.7 on Xe2 (minimum subgroup 16)", VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, "Mesa 25.2.7", 16u, true, false, false},
    {"NVIDIA", VK_DRIVER_ID_NVIDIA_PROPRIETARY, "NVIDIA 580.95.05", 32u, true, false, false},
    {"RADV, Mesa 25.2.7", VK_DRIVER_ID_MESA_RADV, "radv Mesa 25.2.7", 32u, true, false, false},
    {"AMD's proprietary driver", VK_DRIVER_ID_AMD_PROPRIETARY, "25.10.1", 32u, true, false, false},
    {"lavapipe, Mesa 25.2.8", VK_DRIVER_ID_MESA_LLVMPIPE, "llvmpipe Mesa 25.2.8 (LLVM 20.1.2)", 8u, true, false, false},
    {"lavapipe without full compute subgroups", VK_DRIVER_ID_MESA_LLVMPIPE, "llvmpipe Mesa 25.2.8 (LLVM 20.1.2)", 8u, false, false, false},
    {"Intel's Windows driver without full compute subgroups", VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, "Intel Corporation 101.9150", 8u, false, false, false},
    {"ANV, Mesa 25.2.7 without full compute subgroups", VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, "Mesa 25.2.7", 8u, false, false, false},
};

struct Switch {
    AgcDriver::SpannedWaveMode mode;
    bool computeFullSubgroups;
    bool fusedThreadHandles;
    bool all;
    bool tables;
};

constexpr Switch Switches[]{
    {AgcDriver::SpannedWaveMode::FusedThreadTables, true, true, false, true},
    {AgcDriver::SpannedWaveMode::FusedThreadTables, true, false, false, false},
    {AgcDriver::SpannedWaveMode::FusedThreadTables, false, true, false, false},
    {AgcDriver::SpannedWaveMode::All, true, false, true, false},
    {AgcDriver::SpannedWaveMode::All, true, true, true, false},
    {AgcDriver::SpannedWaveMode::All, false, true, false, false},
    {AgcDriver::SpannedWaveMode::Never, true, true, false, false},
    {AgcDriver::SpannedWaveMode::Tables, true, false, false, true},
    {AgcDriver::SpannedWaveMode::Tables, true, true, false, true},
    {AgcDriver::SpannedWaveMode::Tables, false, true, false, false},
};

void RunPolicyTests() {
    for (const auto& item : Detections) {
        const auto result = AgcDriver::FusedThreadHandleSharing(item.driver, item.driverInfo, item.minSubgroupSize, item.computeFullSubgroups);
        Require(result.shared == item.shared, std::string(item.name) + ": expected the driver " + (item.shared ? "to share" : "not to share") + " texture and sampler handles across fused EU threads");
        Require(result.versionUnknown == item.versionUnknown, std::string(item.name) + ": expected the Mesa version to be " + (item.versionUnknown ? "unknown" : "known or not needed"));
    }
    const std::pair<const char*, std::optional<AgcDriver::SpannedWaveMode>> values[]{
        {nullptr, AgcDriver::SpannedWaveMode::FusedThreadTables},
        {"1", AgcDriver::SpannedWaveMode::All},
        {"0", AgcDriver::SpannedWaveMode::Never},
        {"tables", AgcDriver::SpannedWaveMode::Tables},
        {"", std::nullopt},
        {"2", std::nullopt},
        {"yes", std::nullopt},
        {"true", std::nullopt},
        {"TABLES", std::nullopt},
        {"1 ", std::nullopt},
    };
    for (const auto& [text, mode] : values) {
        const std::string name = text == nullptr ? std::string("unset") : "\"" + std::string(text) + "\"";
        Require(AgcDriver::ParseSpannedWaveMode(text) == mode, "APS5_SPANNED_WAVE " + name + (mode ? " was not read as its mode" : " was not refused"));
    }
    for (const auto& item : Switches) {
        const auto targets = AgcDriver::SpannedWaveTargetsFor(item.mode, item.computeFullSubgroups, item.fusedThreadHandles);
        Require(targets.all == item.all && targets.tables == item.tables, "APS5_SPANNED_WAVE mode " + std::to_string(static_cast<int>(item.mode)) + " with full compute subgroups " + std::to_string(item.computeFullSubgroups) + " and fused-thread handles " + std::to_string(item.fusedThreadHandles) + " spans every program " + std::to_string(targets.all) + " and table programs " + std::to_string(targets.tables));
    }
    std::printf("spanned wave policy: %zu drivers, %zu switch values and %zu switch outcomes checked\n", std::size(Detections), std::size(values), std::size(Switches));
}

std::optional<std::string> EmitSpanned(bool guard, const std::function<void(SpirvEmitterState&)>& emit) {
    IrProgram program;
    program.SetWaveSize(64);
    program.Resources().stage = IrShaderStage::Compute;
    SpirvEmitterState state(program, {});
    state.spannedWave = true;
    state.spannedExchangeGuard = guard;
    state.subgroupLocalInvocationIdVariable = state.module.AllocateId();
    state.subgroupIdVariable = state.module.AllocateId();
    std::optional<std::string> refused;
    try {
        emit(state);
    } catch (const SpannedWaveUnsupported& error) {
        refused = error.what();
    }
    Require(state.perLaneConstructs.empty(), "the per-lane construct stack was not unwound");
    return refused;
}

void Ballot(SpirvEmitterState& state) {
    EmitSubgroupBallot(state, state.module.AllocateId(), ConstantBool(state, true));
}

void Shuffle(SpirvEmitterState& state) {
    EmitSubgroupShuffle(state, TypeU32(state), state.module.AllocateId(), ConstantU32(state, 7u), ConstantU32(state, 33u));
}

struct GuardCase {
    const char* name;
    std::function<void(SpirvEmitterState&)> emit;
    const char* construct;
};

void RunGuardTests() {
    const auto condition = [](SpirvEmitterState& state) { return ConstantBool(state, true); };
    const GuardCase cases[]{
        {"a ballot outside any construct", [](SpirvEmitterState& state) { Ballot(state); }, nullptr},
        {"a shuffle outside any construct", [](SpirvEmitterState& state) { Shuffle(state); }, nullptr},
        {"a ballot inside a selection", [&](SpirvEmitterState& state) { EmitIfCondition(state, condition(state), [&] { Ballot(state); }); }, "a per-invocation selection"},
        {"a shuffle inside the value arm of a selection", [&](SpirvEmitterState& state) { static_cast<void>(EmitValueOrZeroIfCondition(state, condition(state), [&] { Shuffle(state); return ConstantU32(state, 0u); })); }, "a per-invocation selection"},
        {"a shuffle inside the else arm of a selection", [&](SpirvEmitterState& state) { static_cast<void>(EmitValueIfElse(state, condition(state), TypeU32(state), [&] { return ConstantU32(state, 0u); }, [&] { Shuffle(state); return ConstantU32(state, 1u); })); }, "a per-invocation selection"},
        {"a ballot inside a compare-exchange retry loop", [&](SpirvEmitterState& state) { static_cast<void>(AtomicUpdate(state, state.module.AllocateId(), ResourceKind::Image, [&](std::uint32_t observed) { Ballot(state); return observed; })); }, "a compare-exchange retry loop"},
        {"a ballot inside a selection inside a compare-exchange retry loop", [&](SpirvEmitterState& state) { static_cast<void>(AtomicUpdate(state, state.module.AllocateId(), ResourceKind::Image, [&](std::uint32_t observed) { EmitIfCondition(state, condition(state), [&] { Ballot(state); }); return observed; })); }, "a per-invocation selection"},
        {"a ballot after a selection", [&](SpirvEmitterState& state) { EmitIfCondition(state, condition(state), [] {}); Ballot(state); }, nullptr},
    };
    for (const auto& item : cases) {
        const auto narrow = EmitSpanned(true, item.emit);
        if (item.construct == nullptr) {
            Require(!narrow.has_value(), std::string(item.name) + ": the guard refused a spanned exchange that every invocation reaches: " + narrow.value_or(""));
        } else {
            Require(narrow.has_value(), std::string(item.name) + ": the guard let a spanned exchange inside a per-lane construct through");
            Require(narrow->find(item.construct) != std::string::npos, std::string(item.name) + ": the refusal does not name " + item.construct + ": " + *narrow);
        }
        const auto all = EmitSpanned(false, item.emit);
        Require(!all.has_value(), std::string(item.name) + ": a span chosen for every program refused the exchange: " + all.value_or(""));
    }
    std::printf("spanned wave guard: %zu exchanges checked with and without the guard\n", std::size(cases));
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::strcmp(argv[1], "--policy") == 0) {
            RunPolicyTests();
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--guard") == 0) {
            RunGuardTests();
            return 0;
        }
        std::fprintf(stderr, "usage: %s --policy | --guard\n", argv[0]);
        return 2;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
