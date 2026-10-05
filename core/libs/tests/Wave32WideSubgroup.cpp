#include "SpirvBackend/SpirvEmitter.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

namespace {

using Value = std::array<std::uint32_t, 4>;
using Lanes = std::vector<Value>;

void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

class Subgroup {
public:
    explicit Subgroup(std::uint32_t size) : size(size) {}

    void Set(std::uint32_t id, Lanes lanes) {
        values[id] = std::move(lanes);
    }

    const Lanes& Get(std::uint32_t id) const {
        return values.at(id);
    }

    void Execute(std::span<const std::uint32_t> code) {
        for (std::size_t offset = 5; offset < code.size();) {
            const auto count = code[offset] >> 16u;
            Require(count != 0 && count <= code.size() - offset, "malformed SPIR-V instruction");
            const auto words = code.subspan(offset, count);
            const auto op = static_cast<spv::Op>(words[0] & 0xffffu);
            offset += count;
            switch (op) {
            case spv::OpTypeInt:
            case spv::OpTypeBool:
            case spv::OpTypeVector:
                continue;
            case spv::OpStore:
                values[words[1]] = values.at(words[2]);
                continue;
            default:
                break;
            }
            auto& result = values[words[2]];
            result.resize(size);
            for (std::uint32_t lane = 0; lane < size; ++lane) {
                const auto arg = [&](std::size_t index) -> const Value& { return values.at(words[index]).at(lane); };
                auto& value = result[lane];
                switch (op) {
                case spv::OpConstant: value[0] = words[3]; break;
                case spv::OpConstantTrue: value[0] = 1; break;
                case spv::OpConstantFalse: value[0] = 0; break;
                case spv::OpLoad: value = arg(3); break;
                case spv::OpBitwiseAnd: value[0] = arg(3)[0] & arg(4)[0]; break;
                case spv::OpBitwiseOr: value[0] = arg(3)[0] | arg(4)[0]; break;
                case spv::OpBitwiseXor: value[0] = arg(3)[0] ^ arg(4)[0]; break;
                case spv::OpIAdd: value[0] = arg(3)[0] + arg(4)[0]; break;
                case spv::OpIMul: value[0] = arg(3)[0] * arg(4)[0]; break;
                case spv::OpShiftRightLogical: value[0] = arg(3)[0] >> arg(4)[0]; break;
                case spv::OpShiftLeftLogical: value[0] = arg(3)[0] << arg(4)[0]; break;
                case spv::OpINotEqual: value[0] = arg(3)[0] != arg(4)[0]; break;
                case spv::OpIEqual: value[0] = arg(3)[0] == arg(4)[0]; break;
                case spv::OpULessThan: value[0] = arg(3)[0] < arg(4)[0]; break;
                case spv::OpLogicalAnd: value[0] = arg(3)[0] && arg(4)[0]; break;
                case spv::OpSelect: value = arg(3)[0] ? arg(4) : arg(5); break;
                case spv::OpCompositeExtract: value[0] = arg(3).at(words[4]); break;
                case spv::OpVectorExtractDynamic: value[0] = arg(3).at(arg(4)[0]); break;
                case spv::OpCompositeConstruct:
                    for (std::size_t component = 0; component < count - 3u; ++component) value.at(component) = arg(3u + component)[0];
                    break;
                case spv::OpGroupNonUniformBallot:
                    value.fill(0);
                    for (std::uint32_t source = 0; source < size; ++source) {
                        if (values.at(words[4])[source][0]) value[source / 32u] |= 1u << (source % 32u);
                    }
                    break;
                case spv::OpGroupNonUniformBallotFindLSB:
                    value[0] = 0xffffffffu;
                    for (std::uint32_t component = 0; component < 4; ++component) {
                        if (arg(4)[component] != 0) {
                            value[0] = component * 32u + std::countr_zero(arg(4)[component]);
                            break;
                        }
                    }
                    break;
                case spv::OpGroupNonUniformShuffle:
                    Require(arg(5)[0] < size, "shuffle escaped the host subgroup");
                    value = values.at(words[4]).at(arg(5)[0]);
                    break;
                default:
                    throw std::runtime_error("unexpected opcode in subgroup test: " + std::to_string(op));
                }
            }
        }
    }

private:
    std::uint32_t size;
    std::unordered_map<std::uint32_t, Lanes> values;
};

void Check(std::uint32_t size, const Value& masks) {
    IrProgram program;
    program.SetWaveSize(32);
    SpirvEmitterState state(program, {});
    state.splitSubgroup = size > 32;
    state.subgroupLocalInvocationIdVariable = state.module.AllocateId();
    SpirvValueEmitContext context(state);
    context.scratchU32Variable = state.module.AllocateId();
    IrValue source(IrOpcode::GetAttribute, IrType::U32, 1);
    IrValue predicate(IrOpcode::GetExec, IrType::Bool, 2);
    IrValue requestedLane(IrOpcode::GetAttribute, IrType::U32, 3);
    IrValue first(IrOpcode::ReadFirstLane, IrType::U32, 4);
    first.AddArgument(&source);
    first.AddArgument(&predicate);
    IrValue read(IrOpcode::ReadLane, IrType::U32, 5);
    read.AddArgument(&source);
    read.AddArgument(&requestedLane);
    IrValue control(IrOpcode::Void, IrType::U32, 6);
    control.SetImmediateU32(0x7c1fu);
    IrValue swizzle(IrOpcode::SwizzleU32, IrType::U32, 7);
    swizzle.AddArgument(&source);
    swizzle.AddArgument(&control);
    swizzle.AddArgument(&predicate);
    IrValue replacement(IrOpcode::Void, IrType::U32, 8);
    replacement.SetImmediateU32(0x12345678u);
    IrValue writeTarget(IrOpcode::Void, IrType::U32, 9);
    writeTarget.SetImmediateU32(7u);
    IrValue write(IrOpcode::WriteLane, IrType::U32, 10);
    write.AddArgument(&replacement);
    write.AddArgument(&writeTarget);
    write.AddArgument(&source);
    const auto sourceId = context.Def(&source);
    const auto predicateId = context.Def(&predicate);
    const auto requestedId = context.Def(&requestedLane);
    const auto logicalId = EmitSubgroupLocalInvocationId(state);
    const auto ballot = context.Ballot(&predicate);
    const auto readFirst = EmitReadFirstLane(context, first);
    const auto readLane = EmitReadLane(context, read);
    const auto active = EmitBallotLaneActiveBool(state, ballot, requestedId);
    const auto quad = EmitDppGroupPermTargetLane(state, logicalId, 0x1bu, 2u);
    const auto quadValue = context.Shuffle(read, 0, quad.lane);
    const auto hardwareActive = EmitSubgroupLaneActiveBool(state, requestedId);
    const auto swizzled = EmitSwizzleU32(context, swizzle);
    const auto written = EmitWriteLane(context, write);
    Subgroup subgroup(size);
    Lanes host(size), sources(size), predicates(size), requested(size);
    for (std::uint32_t lane = 0; lane < size; ++lane) {
        host[lane][0] = lane;
        sources[lane][0] = 0x1000u + lane;
        predicates[lane][0] = (masks[lane / 32u] >> (lane % 32u)) & 1u;
        requested[lane][0] = 31u - lane % 32u;
    }
    subgroup.Set(state.subgroupLocalInvocationIdVariable, host);
    subgroup.Set(sourceId, sources);
    subgroup.Set(predicateId, predicates);
    subgroup.Set(requestedId, requested);
    subgroup.Execute(state.module.Finalize());
    for (std::uint32_t lane = 0; lane < size; ++lane) {
        const auto mask = masks[lane / 32u];
        const auto base = lane / 32u * 32u;
        Require(subgroup.Get(logicalId)[lane][0] == lane % 32u, "lane id escaped its wave32");
        Require(subgroup.Get(ballot)[lane] == Value{mask, 0, 0, 0}, "ballot mixed independent wave32s");
        Require(subgroup.Get(readFirst)[lane][0] == 0x1000u + base + (mask == 0 ? 0u : std::countr_zero(mask)), "readfirstlane read a different wave32");
        Require(subgroup.Get(readLane)[lane][0] == 0x1000u + base + requested[lane][0], "readlane read a different wave32");
        Require(subgroup.Get(active)[lane][0] == ((mask >> requested[lane][0]) & 1u), "source lane activity came from another wave32");
        Require(subgroup.Get(quadValue)[lane][0] == 0x1000u + (lane & ~3u) + 3u - lane % 4u, "DPP quad permutation crossed waves");
        Require(subgroup.Get(hardwareActive)[lane][0] == 1u, "upper wave32 lanes were treated as inactive");
        Require(subgroup.Get(swizzled)[lane][0] == (((mask >> requested[lane][0]) & 1u) != 0 ? 0x1000u + base + requested[lane][0] : 0u), "DS swizzle read another wave32 or ignored source EXEC");
        Require(subgroup.Get(written)[lane][0] == (lane % 32u == 7u ? 0x12345678u : 0x1000u + lane), "writelane missed the upper wave32");
    }
}

}

int main() {
    try {
        for (std::uint32_t size : {32u, 64u, 128u}) {
            for (const Value& masks : {Value{0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu}, Value{1, 0x80000000u, 0x100, 0x4000}, Value{0x55555555u, 0xaaaaaaaau, 0x12345678u, 0xfedcba98u}, Value{0, 1, 0, 0x80000000u}, Value{1, 0, 0x80000000u, 0}, Value{0, 0, 0, 0}}) {
                Check(size, masks);
            }
        }
        std::puts("wave32 wide subgroup tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
