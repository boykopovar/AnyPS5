#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/Structurizer.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Translation/ShaderInputInfoBuilder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t Lanes = 64;

using LaneWords = std::array<std::uint32_t, Lanes>;

struct Value {
    std::array<LaneWords, 4> component{};
};

class Evaluator {
public:
    Evaluator(const IrProgram& program, std::uint32_t waveSize, std::uint32_t hostSubgroup = 0u)
        : lanes(waveSize), hostSubgroup(hostSubgroup == 0u ? waveSize : hostSubgroup), exec(waveSize >= 64u ? ~0ull : (1ull << waveSize) - 1ull) {
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) vector[0][lane] = lane;
        for (const auto& block : program.Blocks()) {
            for (const auto* inst : block->Instructions()) run(*inst);
        }
    }

    LaneWords output{};
    bool exported = false;
    std::map<std::uint32_t, bool> maskTag;
    std::map<std::uint32_t, bool> waveMaskTag;
    std::map<std::uint32_t, const IrValue*> threadBitValue;

    LaneWords vectorRegister(std::uint32_t index) const {
        return vector.at(index);
    }

private:
    std::unordered_map<const IrValue*, Value> values;
    std::array<LaneWords, 256> vector{};
    std::array<LaneWords, 128> scalar{};
    std::map<std::uint32_t, LaneWords> threadBit;
    Value execLo{};
    Value execHi{};
    Value vccLo{};
    Value vccHi{};
    std::uint32_t lanes;
    std::uint32_t hostSubgroup;
    std::uint64_t exec;
    std::uint64_t vcc = 0;
    bool vccWaveMaskTag = false;
    Value scc{};

    static std::uint32_t registerIndex(const IrValue& inst) {
        return inst.Argument(0)->Register().index;
    }

    Value operand(const IrValue* argument) {
        argument = argument->Resolve();
        const auto found = values.find(argument);
        if (found != values.end()) return found->second;
        if (!argument->HasImmediate()) throw std::runtime_error("the evaluator met a value that has not been computed");
        Value constant{};
        const std::uint32_t word = argument->Type() == IrType::Bool ? (argument->ImmediateBool() ? 1u : 0u) : argument->ImmediateU32();
        constant.component[0].fill(word);
        return constant;
    }

    Value fromBits(std::uint64_t bits) const {
        Value result{};
        for (std::uint32_t lane = 0; lane < lanes; ++lane) result.component[0][lane] = static_cast<std::uint32_t>((bits >> lane) & 1u);
        return result;
    }

    std::uint64_t toBits(const Value& value) const {
        std::uint64_t bits = 0;
        for (std::uint32_t lane = 0; lane < lanes; ++lane) bits |= static_cast<std::uint64_t>(value.component[0][lane] & 1u) << lane;
        return bits;
    }

    static Value uniform(std::uint32_t word) {
        Value result{};
        result.component[0].fill(word);
        return result;
    }

    std::uint32_t uniformWord(const Value& value) const {
        for (std::uint32_t lane = 1; lane < lanes; ++lane) {
            if (value.component[0][lane] != value.component[0][0]) throw std::runtime_error("a scalar register was written with a value that differs between lanes");
        }
        return value.component[0][0];
    }

    template <typename TOperation>
    Value lanewise(const IrValue& inst, TOperation operation) {
        const Value left = operand(inst.Argument(0));
        const Value right = operand(inst.Argument(1));
        Value result{};
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) result.component[0][lane] = operation(left.component[0][lane], right.component[0][lane]);
        return result;
    }

    Value select(const IrValue& inst) {
        const Value condition = operand(inst.Argument(0));
        const Value active = operand(inst.Argument(1));
        const Value inactive = operand(inst.Argument(2));
        Value result{};
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            result.component[0][lane] = (condition.component[0][lane] & 1u) != 0u ? active.component[0][lane] : inactive.component[0][lane];
        }
        return result;
    }

    void run(const IrValue& inst) {
        switch (inst.Opcode()) {
        case IrOpcode::SetExec: exec = toBits(operand(inst.Argument(0))); return;
        case IrOpcode::SetExecLo: execLo = operand(inst.Argument(0)); return;
        case IrOpcode::SetExecHi: execHi = operand(inst.Argument(0)); return;
        case IrOpcode::GetExec: values[&inst] = fromBits(exec); return;
        case IrOpcode::GetExecLo: values[&inst] = execLo; return;
        case IrOpcode::GetExecHi: values[&inst] = execHi; return;
        case IrOpcode::SetVcc: vcc = toBits(operand(inst.Argument(0))); return;
        case IrOpcode::SetVccWaveMaskTag: vccWaveMaskTag = (uniformWord(operand(inst.Argument(0))) & 1u) != 0u; return;
        case IrOpcode::SetVccLo: vccLo = operand(inst.Argument(0)); return;
        case IrOpcode::SetVccHi: vccHi = operand(inst.Argument(0)); return;
        case IrOpcode::GetVcc: values[&inst] = fromBits(vcc); return;
        case IrOpcode::GetVccWaveMaskTag: values[&inst] = uniform(vccWaveMaskTag ? 1u : 0u); return;
        case IrOpcode::GetVccLo: values[&inst] = vccLo; return;
        case IrOpcode::GetVccHi: values[&inst] = vccHi; return;
        case IrOpcode::SetScc: scc = operand(inst.Argument(0)); return;
        case IrOpcode::GetScc: values[&inst] = scc; return;
        case IrOpcode::SetScalarRegister: scalar.at(registerIndex(inst)) = operand(inst.Argument(1)).component[0]; return;
        case IrOpcode::GetScalarRegister: {
            Value result{};
            result.component[0] = scalar.at(registerIndex(inst));
            values[&inst] = result;
            return;
        }
        case IrOpcode::SetScalarMaskTag: maskTag[registerIndex(inst)] = (uniformWord(operand(inst.Argument(1))) & 1u) != 0u; return;
        case IrOpcode::GetScalarMaskTag: {
            const auto found = maskTag.find(registerIndex(inst));
            values[&inst] = uniform(found != maskTag.end() && found->second ? 1u : 0u);
            return;
        }
        case IrOpcode::SetScalarWaveMaskTag: waveMaskTag[registerIndex(inst)] = (uniformWord(operand(inst.Argument(1))) & 1u) != 0u; return;
        case IrOpcode::GetScalarWaveMaskTag: {
            const auto found = waveMaskTag.find(registerIndex(inst));
            values[&inst] = uniform(found != waveMaskTag.end() && found->second ? 1u : 0u);
            return;
        }
        case IrOpcode::SetThreadBitScalarRegister:
            threadBit[registerIndex(inst)] = operand(inst.Argument(1)).component[0];
            threadBitValue[registerIndex(inst)] = inst.Argument(1)->Resolve();
            return;
        case IrOpcode::GetThreadBitScalarRegister: {
            Value result{};
            const auto found = threadBit.find(registerIndex(inst));
            if (found != threadBit.end()) result.component[0] = found->second;
            values[&inst] = result;
            return;
        }
        case IrOpcode::SetVectorRegister: vector.at(registerIndex(inst)) = operand(inst.Argument(1)).component[0]; return;
        case IrOpcode::GetVectorRegister: {
            Value result{};
            result.component[0] = vector.at(registerIndex(inst));
            values[&inst] = result;
            return;
        }
        case IrOpcode::GetBuiltin: {
            const auto kind = static_cast<StageInputKind>(inst.Argument(0)->ImmediateU32());
            const std::uint32_t component = inst.Argument(1)->ImmediateU32();
            Value result{};
            if (kind == StageInputKind::LocalInvocationIndex || (kind == StageInputKind::LocalInvocationId && component == 0u)) {
                for (std::uint32_t lane = 0; lane < Lanes; ++lane) result.component[0][lane] = lane;
            }
            values[&inst] = result;
            return;
        }
        case IrOpcode::LaneId: {
            Value result{};
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) result.component[0][lane] = lane % hostSubgroup;
            values[&inst] = result;
            return;
        }
        case IrOpcode::Ballot: {
            const Value predicate = operand(inst.Argument(0));
            Value result{};
            for (std::uint32_t lane = 0; lane < lanes; ++lane) {
                const std::uint32_t base = (lane / hostSubgroup) * hostSubgroup;
                std::uint64_t bits = 0;
                for (std::uint32_t offset = 0; offset < hostSubgroup && base + offset < lanes; ++offset) {
                    bits |= static_cast<std::uint64_t>(predicate.component[0][base + offset] & 1u) << offset;
                }
                result.component[0][lane] = static_cast<std::uint32_t>(bits);
                result.component[1][lane] = static_cast<std::uint32_t>(bits >> 32);
            }
            values[&inst] = result;
            return;
        }
        case IrOpcode::CompositeExtractU32x4: {
            const Value source = operand(inst.Argument(0));
            Value result{};
            result.component[0] = source.component.at(operand(inst.Argument(1)).component[0][0]);
            values[&inst] = result;
            return;
        }
        case IrOpcode::CompositeConstructU32x4: {
            Value result{};
            for (std::uint32_t index = 0; index < 4; ++index) result.component[index] = operand(inst.Argument(index)).component[0];
            values[&inst] = result;
            return;
        }
        case IrOpcode::UGreaterThan32: values[&inst] = lanewise(inst, [](std::uint32_t a, std::uint32_t b) { return a > b ? 1u : 0u; }); return;
        case IrOpcode::ULessThan32: values[&inst] = lanewise(inst, [](std::uint32_t a, std::uint32_t b) { return a < b ? 1u : 0u; }); return;
        case IrOpcode::IEqual32: values[&inst] = lanewise(inst, [](std::uint32_t a, std::uint32_t b) { return a == b ? 1u : 0u; }); return;
        case IrOpcode::INotEqual32: values[&inst] = lanewise(inst, [](std::uint32_t a, std::uint32_t b) { return a != b ? 1u : 0u; }); return;
        case IrOpcode::LogicalAnd: values[&inst] = lanewise(inst, [](std::uint32_t a, std::uint32_t b) { return (a & b) & 1u; }); return;
        case IrOpcode::BitwiseAnd32: values[&inst] = lanewise(inst, [](std::uint32_t a, std::uint32_t b) { return a & b; }); return;
        case IrOpcode::BitwiseOr32: values[&inst] = lanewise(inst, [](std::uint32_t a, std::uint32_t b) { return a | b; }); return;
        case IrOpcode::BitwiseXor32: values[&inst] = lanewise(inst, [](std::uint32_t a, std::uint32_t b) { return a ^ b; }); return;
        case IrOpcode::ShiftRightLogical32: values[&inst] = lanewise(inst, [](std::uint32_t a, std::uint32_t b) { return a >> (b & 31u); }); return;
        case IrOpcode::SelectU1:
        case IrOpcode::SelectU32: values[&inst] = select(inst); return;
        case IrOpcode::SetAttribute:
            output = operand(inst.Argument(0)).component[0];
            exported = true;
            return;
        default: throw std::runtime_error("the evaluator does not support IR opcode " + std::to_string(static_cast<unsigned>(inst.Opcode())));
        }
    }
};

constexpr std::uint32_t Vcc = 0x6au;
constexpr std::uint32_t Exec = 0x7eu;
constexpr std::uint32_t Zero = 0x80u;
constexpr std::uint32_t MinusOne = 0xc1u;
constexpr std::uint32_t S4 = 0x04u;
constexpr std::uint32_t Literal = 0xffu;

std::uint32_t vcmpLanesBelow(std::uint32_t count) {
    return 0x7d880080u + count;
}

std::uint32_t sCmpScc(bool value) {
    return value ? 0xbf068080u : 0xbf068180u;
}

std::uint32_t sCselectB64(std::uint32_t destination, std::uint32_t trueSource, std::uint32_t falseSource) {
    return 0x85800000u | (destination << 16) | (falseSource << 8) | trueSource;
}

std::uint32_t sMovB32(std::uint32_t destination, std::uint32_t source) {
    return 0xbe800300u | (destination << 16) | source;
}

std::uint32_t sMovB64(std::uint32_t destination, std::uint32_t source) {
    return 0xbe800400u | (destination << 16) | source;
}

std::uint32_t sXorB32(std::uint32_t destination, std::uint32_t left, std::uint32_t right) {
    return 0x89000000u | (destination << 16) | (right << 8) | left;
}

std::array<std::uint32_t, 2> vCndmaskV1(std::uint32_t mask) {
    return {0xd5010001u, 0x80u | (129u << 9) | (mask << 18)};
}

struct Outcome {
    LaneWords lanes{};
    std::uint32_t waveSize = 64;
    bool maskTagS4 = false;
    bool constantSources = false;
};

Outcome run(const std::vector<std::uint32_t>& body, std::uint32_t mask, std::uint32_t waveSize = 64) {
    std::vector<std::uint32_t> code = body;
    const auto select = vCndmaskV1(mask);
    code.insert(code.end(), {select[0], select[1], 0xf80008cfu, 0x01010101u, 0xbf810000u});
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    auto cfg = GraphBuilder{}.Build(decoded);
    Structurizer{}.Structurize(cfg);
    GuestContext context{};
    context.waveSize = waveSize;
    context.vertex = ShaderVertexStageInfo{};
    TranslateOptions options{};
    options.stage = ShaderStageKind::Vertex;
    options.waveSize = waveSize;
    options.userDataCount = 0;
    options.inputInfo = BuildShaderStageInputInfo(ShaderStageKind::Vertex, context, waveSize);
    const auto program = InstructionTranslator{}.Translate(decoded, cfg, options);
    const Evaluator evaluator(program, waveSize);
    if (!evaluator.exported) throw std::runtime_error("the program exported nothing");
    Outcome outcome;
    outcome.lanes = evaluator.output;
    outcome.waveSize = waveSize;
    const auto tag = evaluator.maskTag.find(S4);
    outcome.maskTagS4 = tag != evaluator.maskTag.end() && tag->second;
    const auto bit = evaluator.threadBitValue.find(S4);
    if (bit != evaluator.threadBitValue.end() && bit->second->Opcode() == IrOpcode::SelectU1) {
        outcome.constantSources = bit->second->Argument(1)->Resolve()->HasImmediate() && bit->second->Argument(2)->Resolve()->HasImmediate();
    }
    return outcome;
}

Outcome runNarrowCompute(const std::vector<std::uint32_t>& body, std::uint32_t mask) {
    constexpr std::uint32_t WaveSize = 32u;
    constexpr std::uint32_t HostSubgroup = 8u;
    std::vector<std::uint32_t> code = body;
    const auto select = vCndmaskV1(mask);
    code.insert(code.end(), {select[0], select[1], 0xbf810000u});
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    auto cfg = GraphBuilder{}.Build(decoded);
    Structurizer{}.Structurize(cfg);
    GuestContext context{};
    context.waveSize = WaveSize;
    context.compute = ShaderComputeStageInfo{{WaveSize, 1u, 1u}, 0u, {false, false, false}, false, 1};
    TranslateOptions options{};
    options.stage = ShaderStageKind::Compute;
    options.waveSize = WaveSize;
    options.userDataCount = 0;
    options.inputInfo = BuildShaderStageInputInfo(ShaderStageKind::Compute, context, HostSubgroup);
    const auto program = InstructionTranslator{}.Translate(decoded, cfg, options);
    bool usesGuestLane = false;
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            usesGuestLane |= inst->Opcode() == IrOpcode::GetBuiltin && inst->Arguments().size() >= 1u &&
                inst->Argument(0)->Resolve()->HasImmediate() &&
                inst->Argument(0)->Resolve()->ImmediateU32() == static_cast<std::uint32_t>(StageInputKind::LocalInvocationIndex);
        }
    }
    if (!usesGuestLane) throw std::runtime_error("narrow Wave32 mask translation did not request the guest lane index");
    const Evaluator evaluator(program, WaveSize, HostSubgroup);
    Outcome outcome;
    outcome.lanes = evaluator.vectorRegister(1u);
    outcome.waveSize = WaveSize;
    const auto tag = evaluator.maskTag.find(S4);
    outcome.maskTagS4 = tag != evaluator.maskTag.end() && tag->second;
    return outcome;
}

void expectNarrowVccBranchReadsRawWord() {
    constexpr std::uint32_t WaveSize = 32u;
    constexpr std::uint32_t HostSubgroup = 8u;
    const std::vector<std::uint32_t> code{
        sMovB32(Vcc, Literal), 1u,
        0xbf860001u,
        0xbf800000u,
        0xbf810000u,
    };
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    auto cfg = GraphBuilder{}.Build(decoded);
    Structurizer{}.Structurize(cfg);
    GuestContext context{};
    context.waveSize = WaveSize;
    context.compute = ShaderComputeStageInfo{{WaveSize, 1u, 1u}, 0u, {false, false, false}, false, 1};
    TranslateOptions options{};
    options.stage = ShaderStageKind::Compute;
    options.waveSize = WaveSize;
    options.userDataCount = 0;
    options.inputInfo = BuildShaderStageInputInfo(ShaderStageKind::Compute, context, HostSubgroup);
    const auto program = InstructionTranslator{}.Translate(decoded, cfg, options);
    bool readsCompleteTag = false;
    bool readsRawVcc = false;
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            readsCompleteTag |= inst->Opcode() == IrOpcode::GetVccWaveMaskTag;
            readsRawVcc |= inst->Opcode() == IrOpcode::GetVccLo;
        }
    }
    if (!readsCompleteTag || !readsRawVcc) {
        throw std::runtime_error("narrow Wave32 VCCZ branch did not consult complete-mask provenance and the raw VCC word");
    }
}

int failures = 0;

void expectLanes(const char* name, const Outcome& outcome, std::uint64_t expectedBits) {
    for (std::uint32_t lane = 0; lane < outcome.waveSize; ++lane) {
        const std::uint32_t expected = static_cast<std::uint32_t>((expectedBits >> lane) & 1u);
        if (outcome.lanes[lane] != expected) {
            std::fprintf(stderr, "%s: lane %u holds %u, expected %u (expected mask 0x%016llx)\n", name, lane, outcome.lanes[lane], expected, static_cast<unsigned long long>(expectedBits));
            ++failures;
            return;
        }
    }
}

void expectMaskPath(const char* name, const Outcome& outcome) {
    if (!outcome.maskTagS4) {
        std::fprintf(stderr, "%s: the destination pair was not marked as a mask\n", name);
        ++failures;
    }
}

void expectConstantSources(const char* name, const Outcome& outcome, bool expected) {
    if (outcome.constantSources != expected) {
        std::fprintf(stderr, "%s: the sources of the select are %sconstants\n", name, expected ? "not " : "");
        ++failures;
    }
}

constexpr std::uint64_t AllLanes = ~0ull;

std::uint64_t below(std::uint32_t count) {
    return count >= 64u ? AllLanes : (1ull << count) - 1ull;
}

void selectCase(const char* name, std::vector<std::uint32_t> setup, std::uint32_t trueSource, std::uint32_t falseSource, std::uint64_t whenSccSet, std::uint64_t whenSccClear, bool maskPath, int constantSources = -1) {
    for (const bool sccValue : {true, false}) {
        auto body = setup;
        body.push_back(sCmpScc(sccValue));
        body.push_back(sCselectB64(4u, trueSource, falseSource));
        const auto outcome = run(body, S4);
        const std::string label = std::string(name) + (sccValue ? ", scc = 1" : ", scc = 0");
        expectLanes(label.c_str(), outcome, sccValue ? whenSccSet : whenSccClear);
        if (maskPath) expectMaskPath(label.c_str(), outcome);
        if (constantSources >= 0) expectConstantSources(label.c_str(), outcome, constantSources != 0);
    }
}

}

int main() {
    try {
        constexpr std::uint32_t NarrowMask = 0x96a596a5u;
        expectNarrowVccBranchReadsRawWord();
        expectLanes("wave32 subgroup8 literal vcc",
            runNarrowCompute({sMovB32(Vcc, Literal), NarrowMask}, Vcc), NarrowMask);
        expectLanes("wave32 subgroup8 scalar xor",
            runNarrowCompute({sMovB32(S4, Literal), NarrowMask, sXorB32(S4, S4, Zero), sMovB32(Vcc, S4)}, Vcc), NarrowMask);
        expectLanes("wave32 subgroup8 compare round trip",
            runNarrowCompute({vcmpLanesBelow(9u), sMovB32(S4, Vcc), sXorB32(S4, S4, Zero), sMovB32(Vcc, S4)}, Vcc), below(9u));

        selectCase("vcc, exec", {vcmpLanesBelow(4)}, Vcc, Exec, below(4), AllLanes, true);
        selectCase("exec, vcc", {vcmpLanesBelow(4)}, Exec, Vcc, AllLanes, below(4), true);
        selectCase("wave64 halves, vcc, exec", {vcmpLanesBelow(40)}, Vcc, Exec, below(40), AllLanes, true);
        selectCase("wave64 halves, exec, vcc", {vcmpLanesBelow(40)}, Exec, Vcc, AllLanes, below(40), true);
        selectCase("exec, zero", {}, Exec, Zero, AllLanes, 0ull, true, 0);
        selectCase("zero, exec", {}, Zero, Exec, 0ull, AllLanes, true, 0);
        selectCase("zero, zero", {}, Zero, Zero, 0ull, 0ull, true, 1);
        selectCase("minus one, zero", {}, MinusOne, Zero, AllLanes, 0ull, true, 1);
        selectCase("minus one, exec", {}, MinusOne, Exec, AllLanes, AllLanes, true, 0);
        selectCase("destination is the true source", {vcmpLanesBelow(40), sMovB64(4u, Vcc)}, S4, Exec, below(40), AllLanes, false);
        selectCase("destination is the false source", {vcmpLanesBelow(40), sMovB64(4u, Vcc)}, Exec, S4, AllLanes, below(40), false);
        for (const bool sccValue : {true, false}) {
            const auto outcome = run({sCmpScc(sccValue), sCselectB64(4u, Literal, Exec), 0x0000ffffu}, S4);
            const std::string label = std::string("literal 0xffff, exec") + (sccValue ? ", scc = 1" : ", scc = 0");
            expectLanes(label.c_str(), outcome, sccValue ? below(16) : AllLanes);
        }
        for (const bool sccValue : {true, false}) {
            const auto outcome = run({sCmpScc(sccValue), sCselectB64(4u, Literal, Zero), 0xffffffffu}, S4);
            const std::string label = std::string("wave64, literal all ones, zero") + (sccValue ? ", scc = 1" : ", scc = 0");
            expectLanes(label.c_str(), outcome, sccValue ? below(32) : 0ull);
            expectConstantSources(label.c_str(), outcome, false);
        }
        for (const bool sccValue : {true, false}) {
            const auto outcome = run({sCmpScc(sccValue), sCselectB64(4u, Literal, Zero), 0xffffffffu}, S4, 32);
            const std::string label = std::string("wave32, literal all ones, zero") + (sccValue ? ", scc = 1" : ", scc = 0");
            expectLanes(label.c_str(), outcome, sccValue ? below(32) : 0ull);
            expectConstantSources(label.c_str(), outcome, true);
        }
        for (const bool sccValue : {true, false}) {
            const auto outcome = run({vcmpLanesBelow(40), sCmpScc(sccValue), sCselectB64(Vcc, Vcc, Exec)}, Vcc);
            expectLanes(sccValue ? "destination vcc, scc = 1" : "destination vcc, scc = 0", outcome, sccValue ? below(40) : AllLanes);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return failures == 0 ? 0 : 1;
}
