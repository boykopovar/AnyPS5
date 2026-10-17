#ifndef CORE_SHADER_RECOMPILER_OPTIMIZATION_GPUSRTPROGRAM_HPP
#define CORE_SHADER_RECOMPILER_OPTIMIZATION_GPUSRTPROGRAM_HPP

#include "IntermediateRepresentation/IrMetadata/ResourcePlan.hpp"
#include "Optimization/SrtWalker.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ShaderRecompiler {

enum class GpuSrtOp : std::uint32_t {
    UserData, ShaderBase, Copy, Extract64, AddCarry, Construct64, ReadAddress, ReadBuffer,
    IAdd32, IAdd64, ISub32, ISub64, IMul32, IMul64, SMin32, UMin32, And32, And64, Or32, Xor32, Not32,
    Shl32, Shl64, Shr32, Shr64, Sar32, Sar64, BfeU, BfeS, Bfi, Select, IEq, INe, SLt, ULt, UGt,
    LAnd, LOr, LXor, LNot,
};

struct GpuSrtInstruction {
    GpuSrtOp op = GpuSrtOp::Copy;
    std::uint32_t dst = 0;
    std::array<std::uint32_t, 5> args{};
    std::uint32_t immediate = 0;
    std::uint32_t flags = 0;
    std::uint32_t pc = 0;
    std::uint32_t leaf = 0;
};

inline constexpr std::uint32_t GpuSrtLoadedFromMemory = 1u;

struct GpuSrtProgram {
    std::vector<std::uint64_t> constants;
    std::uint32_t slotCount = 0;
    std::vector<GpuSrtInstruction> code;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> flat;
    std::vector<std::uint32_t> sourceDwords;
    std::vector<std::uint32_t> sourceDwordCounts;
    std::uint32_t flatSize = 0;
};

[[nodiscard]] std::optional<GpuSrtProgram> EncodeGpuSrtProgram(const IrResourcePlan& plan, std::string* reason = nullptr);

struct GpuSrtResult {
    std::vector<std::uint32_t> flat;
    std::vector<DescriptorValue> sources;
    std::uint32_t nullRoots = 0;
    std::vector<SrtReadPoison> poison;
};

[[nodiscard]] bool RunGpuSrtProgram(const GpuSrtProgram& program, const SrtRuntime& runtime, GpuSrtResult& result);

}

#endif
