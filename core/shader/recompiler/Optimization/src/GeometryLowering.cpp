#include "Optimization/GeometryLowering.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t GeometryVertexBases = 6u;
constexpr std::uint32_t EsGsComponentBytes = 256u;
constexpr std::uint32_t GsVsVertexBytes = 64u;

[[noreturn]] void Fail(const char* reason, std::uint32_t pc) {
    char text[320];
    std::snprintf(text, sizeof(text), "geometry lowering: %s at pc 0x%x", reason, pc);
    throw std::runtime_error(text);
}

[[noreturn]] void FailCopy(const std::string& reason, std::uint32_t pc) {
    char text[64];
    std::snprintf(text, sizeof(text), " at pc 0x%x", pc);
    throw std::runtime_error("geometry copy program: " + reason + text);
}

std::optional<std::uint32_t> BaseKind(const IrValue* raw) {
    const IrValue* value = raw->Resolve();
    if (value->Opcode() != IrOpcode::TessellationBase || !value->Argument(0)->Resolve()->HasImmediate()) return std::nullopt;
    return value->Argument(0)->Resolve()->ImmediateU32();
}

bool IsBase(const IrValue* raw, TessellationBaseKind kind) {
    const auto found = BaseKind(raw);
    return found && *found == static_cast<std::uint32_t>(kind);
}

std::optional<std::uint32_t> GeometryVertexOf(const IrValue* raw) {
    const auto kind = BaseKind(raw);
    const auto first = static_cast<std::uint32_t>(TessellationBaseKind::GeometryVertex);
    if (!kind || *kind < first || *kind >= first + GeometryVertexBases) return std::nullopt;
    return *kind - first;
}

bool IsGeometryBase(const IrValue* raw) {
    return IsBase(raw, TessellationBaseKind::EsGsOffset) || IsBase(raw, TessellationBaseKind::GsVsOffset) || GeometryVertexOf(raw).has_value();
}

struct Term {
    std::optional<std::uint32_t> vertex;
    std::int64_t scale = 0;
    std::int64_t constant = 0;
};

std::optional<Term> Evaluate(const IrValue* raw, std::uint32_t depth = 0u) {
    if (depth > 32u) return std::nullopt;
    const IrValue* value = raw->Resolve();
    if (value->HasImmediate()) return Term{std::nullopt, 0, static_cast<std::int64_t>(value->ImmediateU32())};
    if (const auto vertex = GeometryVertexOf(value)) return Term{vertex, 1, 0};
    const auto pair = [&]() -> std::optional<std::pair<Term, Term>> {
        if (value->ArgumentCount() < 2u) return std::nullopt;
        const auto left = Evaluate(value->Argument(0), depth + 1u);
        const auto right = Evaluate(value->Argument(1), depth + 1u);
        if (!left || !right) return std::nullopt;
        return std::pair{*left, *right};
    };
    switch (value->Opcode()) {
    case IrOpcode::IAdd32: {
        const auto terms = pair();
        if (!terms || (terms->first.vertex && terms->second.vertex)) return std::nullopt;
        Term sum{terms->first.vertex ? terms->first.vertex : terms->second.vertex};
        if (__builtin_add_overflow(terms->first.scale, terms->second.scale, &sum.scale) || __builtin_add_overflow(terms->first.constant, terms->second.constant, &sum.constant)) return std::nullopt;
        return sum;
    }
    case IrOpcode::ISub32: {
        const auto terms = pair();
        if (!terms || terms->second.vertex) return std::nullopt;
        Term difference{terms->first.vertex, terms->first.scale};
        if (__builtin_sub_overflow(terms->first.constant, terms->second.constant, &difference.constant)) return std::nullopt;
        return difference;
    }
    case IrOpcode::ShiftLeftLogical32: {
        const auto terms = pair();
        if (!terms || terms->second.vertex || terms->second.constant < 0 || terms->second.constant > 31) return std::nullopt;
        const auto factor = std::int64_t{1} << terms->second.constant;
        Term shifted{terms->first.vertex};
        if (__builtin_mul_overflow(terms->first.scale, factor, &shifted.scale) || __builtin_mul_overflow(terms->first.constant, factor, &shifted.constant)) return std::nullopt;
        return shifted;
    }
    case IrOpcode::IMul32: {
        const auto terms = pair();
        if (!terms || (terms->first.vertex && terms->second.vertex)) return std::nullopt;
        const auto& varying = terms->first.vertex ? terms->first : terms->second;
        const auto& factor = terms->first.vertex ? terms->second : terms->first;
        Term product{varying.vertex};
        if (__builtin_mul_overflow(varying.scale, factor.constant, &product.scale) || __builtin_mul_overflow(varying.constant, factor.constant, &product.constant)) return std::nullopt;
        return product;
    }
    default:
        return std::nullopt;
    }
}

ExportTargetKind CopyTargetKind(std::uint32_t target, std::uint32_t& index) {
    if (target >= 0x0cu && target <= 0x0fu) {
        index = target - 0x0cu;
        return ExportTargetKind::Position;
    }
    if (target >= 0x20u && target <= 0x3fu) {
        index = target - 0x20u;
        return ExportTargetKind::Parameter;
    }
    index = 0u;
    return ExportTargetKind::Unknown;
}

class Lowering {
public:
    Lowering(IrProgram& program, const ShaderGeometryInputInfo& geometry) : program(program), geometry(geometry), stage(program.Resources().stage) {}

    void Run() {
        if (stage != IrShaderStage::Vertex && stage != IrShaderStage::Geometry) Fail("a geometry configuration reached a stage other than the export or geometry program", 0u);
        std::vector<std::pair<IrBlock*, IrValue*>> accesses;
        std::vector<std::pair<IrBlock*, IrValue*>> emits;
        for (IrBlock* block : program.BlockOrder()) {
            for (IrValue* inst : block->Instructions()) {
                if (stage == IrShaderStage::Geometry && SharedAccessOf(inst->Opcode()) != SharedAccess::None) Fail("a GCN geometry program that uses LDS is unsupported", inst->Flags<MemoryFlags>().pc);
                if (inst->Opcode() == IrOpcode::EmitVertex) emits.emplace_back(block, inst);
                if (BufferAccessOf(inst->Opcode()) != BufferAccess::None && inst->ArgumentCount() > 3u && ringAccess(inst)) accesses.emplace_back(block, inst);
            }
        }
        if (stage == IrShaderStage::Vertex && !emits.empty()) Fail("an export program emits geometry vertices", 0u);
        for (const auto& [block, inst] : accesses) rewrite(*block, *inst);
        if (stage == IrShaderStage::Geometry) {
            std::vector<std::uint32_t> exportIndices;
            for (const auto& copy : geometry.exports) exportIndices.push_back(addExport(copy));
            for (const auto& [block, inst] : emits) copyOutputs(*block, *inst, exportIndices);
        }
        constexpr DeadCodeEliminator deadCodeEliminator;
        deadCodeEliminator.Eliminate(program);
        for (IrBlock* block : program.BlockOrder()) {
            for (IrValue* inst : block->Instructions()) {
                if (inst->Opcode() == IrOpcode::TessellationBase && inst->HasUses() && IsGeometryBase(inst)) Fail(GeometryVertexOf(inst) ? "an ES vertex offset is used other than as an ESGS ring read address" : "a geometry ring offset is used other than as a ring access offset", 0u);
            }
        }
        program.Metadata().tessellationLocalStride = geometry.esgsItemDwords * 4u;
    }

private:
    bool ringAccess(IrValue* inst) const {
        const bool soffset = IsGeometryBase(inst->Argument(3));
        const auto offset = Evaluate(inst->Argument(2));
        const bool voffset = offset.has_value() && offset->vertex.has_value();
        const auto memory = program.Resources().memoryInfo.at(inst->Flags<MemoryFlags>().index);
        if (!soffset && !voffset) {
            if (IsGeometryBase(inst->Argument(2))) Fail("a geometry ring offset is used as a buffer vector offset", inst->Flags<MemoryFlags>().pc);
            return false;
        }
        if (memory.kind != ResourceKind::Buffer || BufferComponentCount(inst->Opcode()) == 0u || memory.dataBits != 32u || memory.formatted || memory.typed || memory.d16 || memory.idxen) Fail("only untyped 32-bit geometry ring accesses without an index are supported", inst->Flags<MemoryFlags>().pc);
        return true;
    }

    IrValue& constant(std::uint32_t value) {
        IrValue& result = program.CreateValue(IrOpcode::Void, IrType::U32);
        result.SetImmediateU32(value);
        return result;
    }

    IrValue& insert(IrBlock& block, IrValue& position, IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        IrValue& value = program.CreateValue(opcode, type);
        for (IrValue* argument : arguments) value.AddArgument(argument);
        block.InsertInstructionBefore(&position, &value);
        return value;
    }

    IrValue& storeData(IrBlock& block, IrValue& inst, std::uint32_t components, std::uint32_t component) {
        if (components == 1u) return *inst.Argument(4u);
        const auto extract = components == 2u ? IrOpcode::CompositeExtractU32x2 : components == 3u ? IrOpcode::CompositeExtractU32x3 : IrOpcode::CompositeExtractU32x4;
        return insert(block, inst, extract, IrType::U32, {inst.Argument(4u), &constant(component)});
    }

    void rewrite(IrBlock& block, IrValue& inst) {
        const auto flags = inst.Flags<MemoryFlags>();
        const auto memory = program.Resources().memoryInfo.at(flags.index);
        const auto components = BufferComponentCount(inst.Opcode());
        const bool write = BufferAccessOf(inst.Opcode()) == BufferAccess::Write;
        IrValue* predicate = inst.Argument(inst.ArgumentCount() - 1u);
        if (!write && BufferAccessOf(inst.Opcode()) != BufferAccess::Read) Fail("an atomic geometry ring access is unsupported", flags.pc);
        if (stage == IrShaderStage::Vertex) {
            if (!write || !IsBase(inst.Argument(3), TessellationBaseKind::EsGsOffset)) Fail("an export program reads a geometry ring", flags.pc);
            const auto offset = memory.offen ? Evaluate(inst.Argument(2)) : std::optional<Term>{Term{}};
            if (!offset || offset->vertex) Fail("an ESGS ring store at an address computed per lane is unsupported", flags.pc);
            const auto address = offset->constant + memory.offset;
            if (address < 0 || (address & 3) != 0 || address + 4 * static_cast<std::int64_t>(components) > static_cast<std::int64_t>(geometry.esgsItemDwords) * 4) Fail("an ESGS ring store outside VGT_ESGS_RING_ITEMSIZE", flags.pc);
            for (std::uint32_t component = 0; component < components; ++component) {
                static_cast<void>(insert(block, inst, IrOpcode::SetTessellationAttribute, IrType::Void, {&constant(static_cast<std::uint32_t>(TessellationAttribute::LocalOutput)), &constant(static_cast<std::uint32_t>(address) + 4u * component), &storeData(block, inst, components, component), predicate}));
            }
        } else if (write) {
            if (!IsBase(inst.Argument(3), TessellationBaseKind::GsVsOffset)) Fail("a geometry program writes the ESGS ring", flags.pc);
            const auto offset = memory.offen ? Evaluate(inst.Argument(2)) : std::optional<Term>{Term{}};
            if (offset && offset->vertex) Fail("a GSVS ring store addressed by an ES vertex offset", flags.pc);
            for (std::uint32_t component = 0; component < components; ++component) {
                const auto bytes = memory.offset + 4u * component;
                IrValue* address = nullptr;
                if (offset) {
                    const auto value = offset->constant + bytes;
                    if (value < 0 || (value & 3) != 0) Fail("an unaligned GSVS ring store", flags.pc);
                    if (value / 4 >= static_cast<std::int64_t>(geometry.vertexDwords) * geometry.maxVertices) Fail("a GSVS ring store outside VGT_GS_VERT_ITEMSIZE times VGT_GS_MAX_VERT_OUT dwords", flags.pc);
                    address = &constant(static_cast<std::uint32_t>(value));
                } else {
                    address = bytes == 0u ? inst.Argument(2) : &insert(block, inst, IrOpcode::IAdd32, IrType::U32, {inst.Argument(2), &constant(bytes)});
                }
                static_cast<void>(insert(block, inst, IrOpcode::SetTessellationAttribute, IrType::Void, {&constant(static_cast<std::uint32_t>(TessellationAttribute::GeometryRing)), address, &storeData(block, inst, components, component), predicate}));
            }
        } else {
            if (components != 1u) Fail("only dword ESGS ring reads are supported", flags.pc);
            if (IsGeometryBase(inst.Argument(3))) Fail("an ESGS ring read offset by a geometry ring base", flags.pc);
            const auto offset = memory.offen ? Evaluate(inst.Argument(2)) : std::nullopt;
            const auto scalar = Evaluate(inst.Argument(3));
            if (!offset || !offset->vertex || !scalar || scalar->vertex) Fail("an ESGS ring read whose vertex is not one of the primitive's input vertices", flags.pc);
            if (offset->scale != 4) Fail("an ESGS ring read that does not scale the ES vertex offset by 4", flags.pc);
            if (*offset->vertex >= geometry.inputVertices) Fail("an ESGS ring read of a vertex the input primitive does not have", flags.pc);
            const auto bytes = offset->constant + scalar->constant + memory.offset;
            if (bytes < 0 || bytes % EsGsComponentBytes != 0 || bytes / EsGsComponentBytes >= geometry.esgsItemDwords) Fail("an ESGS ring read outside VGT_ESGS_RING_ITEMSIZE or between components", flags.pc);
            const auto address = *offset->vertex * geometry.esgsItemDwords * 4u + static_cast<std::uint32_t>(bytes / EsGsComponentBytes) * 4u;
            inst.ReplaceAllUsesWith(&insert(block, inst, IrOpcode::GetTessellationAttribute, IrType::U32, {&constant(static_cast<std::uint32_t>(TessellationAttribute::GeometryInput)), &constant(address), predicate}));
        }
        block.RemoveInstruction(&inst);
        inst.Invalidate();
    }

    std::uint32_t addExport(const GeometryCopyExport& copy) {
        ExportInfo info;
        info.kind = CopyTargetKind(copy.target, info.index);
        if (info.kind == ExportTargetKind::Unknown) Fail("the copy program exports a target other than a position or a parameter", 0u);
        info.target = copy.target;
        info.en = copy.enable;
        info.done = copy.done;
        const auto index = static_cast<std::uint32_t>(program.Metadata().exportInfo.size());
        program.Metadata().exportInfo.push_back(info);
        return index;
    }

    void copyOutputs(IrBlock& block, IrValue& emit, const std::vector<std::uint32_t>& exportIndices) {
        if (!emit.Argument(1)->Resolve()->HasImmediate()) Fail("a geometry emit message without an immediate operation", 0u);
        if ((emit.Argument(1)->Resolve()->ImmediateU32() & 2u) == 0u) return;
        IrValue* exec = emit.Argument(0);
        for (std::size_t index = 0; index < geometry.exports.size(); ++index) {
            const auto& copy = geometry.exports[index];
            std::array<IrValue*, 4> values{};
            for (std::uint32_t component = 0; component < 4u; ++component) {
                if ((copy.enable & (1u << component)) == 0u) {
                    values[component] = &constant(0u);
                } else if ((copy.constants & (1u << component)) != 0u) {
                    values[component] = &constant(copy.values[component]);
                } else {
                    values[component] = &insert(block, emit, IrOpcode::GetTessellationAttribute, IrType::U32, {&constant(static_cast<std::uint32_t>(TessellationAttribute::GeometryEmitted)), &constant(copy.values[component] * 4u), exec});
                }
            }
            IrValue& data = insert(block, emit, IrOpcode::CompositeConstructU32x4, IrType::U32x4, {values[0], values[1], values[2], values[3]});
            IrValue& store = insert(block, emit, IrOpcode::SetAttribute, IrType::Void, {&data, exec});
            store.SetFlags(ExportFlags{exportIndices[index], 0u});
        }
    }

    IrProgram& program;
    const ShaderGeometryInputInfo& geometry;
    IrShaderStage stage;
};

struct CopySource {
    bool vertexOffset = false;
    bool constant = false;
    std::optional<std::uint32_t> slot;
    std::uint32_t value = 0;
};

std::optional<std::uint32_t> OperandConstant(const RdnaOperand& operand) {
    if (operand.kind == RdnaOperandKind::IntegerInlineConstant || operand.kind == RdnaOperandKind::FloatInlineConstant || operand.kind == RdnaOperandKind::LiteralConstant) return operand.value;
    return std::nullopt;
}

std::optional<std::uint32_t> ScalarValue(const RdnaOperand& operand, const std::map<std::uint32_t, std::uint32_t>& scalars) {
    if (const auto value = OperandConstant(operand)) return value;
    if (operand.kind != RdnaOperandKind::ScalarRegister) return std::nullopt;
    const auto found = scalars.find(operand.reg);
    return found == scalars.end() ? std::nullopt : std::optional<std::uint32_t>(found->second);
}

std::optional<std::uint32_t> FoldScalar(const RdnaInstruction& instruction, const std::map<std::uint32_t, std::uint32_t>& scalars) {
    if (instruction.is64Bit) return std::nullopt;
    const auto first = ScalarValue(instruction.source0, scalars);
    if (instruction.op == RdnaOpcode::SMovkI32 || instruction.op == RdnaOpcode::SMovB32) return first;
    const auto second = ScalarValue(instruction.source1, scalars);
    if (!first || !second) return std::nullopt;
    switch (instruction.op) {
    case RdnaOpcode::SAddI32:
    case RdnaOpcode::SAddU32: return *first + *second;
    case RdnaOpcode::SMulI32:
    case RdnaOpcode::SMulkI32: return *first * *second;
    case RdnaOpcode::SLshlB32: return *first << (*second & 31u);
    case RdnaOpcode::SLshrB32: return *first >> (*second & 31u);
    case RdnaOpcode::SAndB32: return *first & *second;
    case RdnaOpcode::SOrB32: return *first | *second;
    case RdnaOpcode::SBfmB32: return ((*first & 31u) == 0u ? 0u : (0xffffffffu >> (32u - (*first & 31u)))) << (*second & 31u);
    default: return std::nullopt;
    }
}

std::optional<std::optional<bool>> CompareScalar(const RdnaInstruction& instruction, const std::map<std::uint32_t, std::uint32_t>& scalars) {
    const auto first = ScalarValue(instruction.source0, scalars);
    const auto second = ScalarValue(instruction.source1, scalars);
    const auto known = [&](auto compare) -> std::optional<bool> {
        if (!first || !second) return std::nullopt;
        return compare(*first, *second, static_cast<std::int32_t>(*first), static_cast<std::int32_t>(*second));
    };
    switch (instruction.op) {
    case RdnaOpcode::SCmpEqU32:
    case RdnaOpcode::SCmpEqI32: return known([](auto a, auto b, auto, auto) { return a == b; });
    case RdnaOpcode::SCmpLgU32:
    case RdnaOpcode::SCmpLgI32: return known([](auto a, auto b, auto, auto) { return a != b; });
    case RdnaOpcode::SCmpGtU32: return known([](auto a, auto b, auto, auto) { return a > b; });
    case RdnaOpcode::SCmpGeU32: return known([](auto a, auto b, auto, auto) { return a >= b; });
    case RdnaOpcode::SCmpLtU32: return known([](auto a, auto b, auto, auto) { return a < b; });
    case RdnaOpcode::SCmpLeU32: return known([](auto a, auto b, auto, auto) { return a <= b; });
    case RdnaOpcode::SCmpGtI32: return known([](auto, auto, auto a, auto b) { return a > b; });
    case RdnaOpcode::SCmpGeI32: return known([](auto, auto, auto a, auto b) { return a >= b; });
    case RdnaOpcode::SCmpLtI32: return known([](auto, auto, auto a, auto b) { return a < b; });
    case RdnaOpcode::SCmpLeI32: return known([](auto, auto, auto a, auto b) { return a <= b; });
    default: return std::nullopt;
    }
}

bool KeepsScc(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::SMovB32 || opcode == RdnaOpcode::SMovkI32 || opcode == RdnaOpcode::SBfmB32 || opcode == RdnaOpcode::SMulI32 || opcode == RdnaOpcode::SMulkI32;
}

void Forget(std::map<std::uint32_t, CopySource>& vectors, std::map<std::uint32_t, std::uint32_t>& scalars, const RdnaInstruction& instruction) {
    for (const auto* destination : {&instruction.destination, &instruction.destination2}) {
        const auto count = std::max(instruction.dataDwordCount, 1u) + (instruction.is64Bit ? 1u : 0u);
        for (std::uint32_t reg = 0; reg < count; ++reg) {
            if (destination->kind == RdnaOperandKind::VectorRegister) vectors.erase(destination->reg + reg);
            if (destination->kind == RdnaOperandKind::ScalarRegister) scalars.erase(destination->reg + reg);
        }
    }
}

}

void GeometryLowering::Lower(IrProgram& program, const ShaderGeometryInputInfo& geometry) const {
    Lowering(program, geometry).Run();
}

std::vector<GeometryCopyExport> ParseGeometryCopyProgram(std::span<const std::uint32_t> code, std::uint32_t maxVertices, std::uint32_t vertexDwords, std::optional<std::uint32_t> streamConfigSgpr) {
    if (maxVertices == 0u || vertexDwords == 0u) throw std::runtime_error("geometry copy program: VGT_GS_MAX_VERT_OUT and VGT_GS_VERT_ITEMSIZE must be nonzero");
    const auto program = RdnaInstructionDecoder{}.Decode(code);
    std::map<std::uint32_t, CopySource> vectors;
    std::map<std::uint32_t, std::uint32_t> scalars;
    if (streamConfigSgpr) scalars[*streamConfigSgpr] = 0u;
    bool vertexIndex = true;
    std::optional<bool> scc;
    std::optional<std::uint32_t> skipTo;
    std::vector<GeometryCopyExport> exports;
    const auto slotBytes = maxVertices * GsVsVertexBytes;
    for (const auto& instruction : program.instructions) {
        const auto pc = instruction.programCounter;
        if (skipTo) {
            if (pc < *skipTo) continue;
            if (pc > *skipTo) FailCopy("a branch into the middle of an instruction", pc);
            skipTo.reset();
        }
        if (instruction.op == RdnaOpcode::SEndpgm) break;
        if (instruction.op == RdnaOpcode::SBranch || instruction.op == RdnaOpcode::SCbranchScc0 || instruction.op == RdnaOpcode::SCbranchScc1) {
            if (instruction.branchTarget <= pc) FailCopy("a backward branch in a copy program", pc);
            if (instruction.op != RdnaOpcode::SBranch && !scc) FailCopy("a copy program branch on a condition not known at draw time", pc);
            if (instruction.op == RdnaOpcode::SBranch || *scc == (instruction.op == RdnaOpcode::SCbranchScc1)) skipTo = instruction.branchTarget;
            continue;
        }
        if (IsBranchOpcode(instruction.op) || instruction.op == RdnaOpcode::SSetpcB64 || instruction.op == RdnaOpcode::SSwappcB64 || instruction.op == RdnaOpcode::SCallB64) FailCopy("control flow other than a scalar condition in a copy program", pc);
        if (const auto compared = CompareScalar(instruction, scalars)) {
            scc = *compared;
            continue;
        }
        const auto family = instruction.family;
        if ((family == RdnaInstructionFamily::SOP1 || family == RdnaInstructionFamily::SOP2 || family == RdnaInstructionFamily::SOPK || family == RdnaInstructionFamily::SOPC) && !KeepsScc(instruction.op)) scc.reset();
        for (const auto* destination : {&instruction.destination, &instruction.destination2}) {
            if (destination->kind == RdnaOperandKind::ExecLo || destination->kind == RdnaOperandKind::ExecHi) FailCopy("a copy program that writes EXEC", pc);
        }
        const bool indexLive = vertexIndex;
        for (const auto* destination : {&instruction.destination, &instruction.destination2}) {
            if (destination->kind == RdnaOperandKind::VectorRegister && destination->reg == 0u) vertexIndex = false;
        }
        if (const auto folded = FoldScalar(instruction, scalars)) {
            Forget(vectors, scalars, instruction);
            if (instruction.destination.kind == RdnaOperandKind::ScalarRegister) scalars[instruction.destination.reg] = *folded;
            continue;
        }
        if (instruction.op == RdnaOpcode::VLshlrevB32 && instruction.source1.kind == RdnaOperandKind::VectorRegister && instruction.source1.reg == 0u && OperandConstant(instruction.source0) == 2u && indexLive) {
            Forget(vectors, scalars, instruction);
            vectors[instruction.destination.reg] = CopySource{true};
            continue;
        }
        if (instruction.op == RdnaOpcode::VMovB32) {
            const auto value = OperandConstant(instruction.source0);
            const auto source = instruction.source0.kind == RdnaOperandKind::VectorRegister ? vectors.find(instruction.source0.reg) : vectors.end();
            const auto copied = source != vectors.end() ? std::optional<CopySource>(source->second) : std::nullopt;
            Forget(vectors, scalars, instruction);
            if (value) vectors[instruction.destination.reg] = CopySource{false, true, std::nullopt, *value};
            else if (copied) vectors[instruction.destination.reg] = *copied;
            continue;
        }
        if (instruction.op == RdnaOpcode::BufferLoadDword && instruction.family == RdnaInstructionFamily::MUBUF) {
            const auto address = vectors.find(instruction.source0.reg);
            const bool vertexOffset = address != vectors.end() && address->second.vertexOffset;
            Forget(vectors, scalars, instruction);
            if (!instruction.offen || instruction.idxen || !vertexOffset) FailCopy("a GSVS ring read not addressed by the vertex index times 4", pc);
            const auto soffset = ScalarValue(instruction.source2, scalars);
            if (!soffset) FailCopy("a GSVS ring read whose scalar offset is not a constant", pc);
            const auto bytes = static_cast<std::uint64_t>(*soffset) + instruction.memoryOffset;
            if (bytes % slotBytes != 0u || bytes / slotBytes >= vertexDwords) FailCopy("a GSVS ring read at " + std::to_string(bytes) + " bytes, which is not one of the " + std::to_string(vertexDwords) + " vertex dwords of " + std::to_string(slotBytes) + " bytes each", pc);
            vectors[instruction.destination.reg] = CopySource{false, false, static_cast<std::uint32_t>(bytes / slotBytes)};
            continue;
        }
        if (instruction.op == RdnaOpcode::Exp) {
            if (instruction.exportIsCompressed) FailCopy("a compressed export", pc);
            GeometryCopyExport copy;
            copy.target = instruction.exportTarget;
            copy.enable = instruction.exportEnableMask & 0xfu;
            copy.done = instruction.exportIsLast;
            std::uint32_t index = 0;
            if (CopyTargetKind(copy.target, index) == ExportTargetKind::Unknown) FailCopy("an export to target " + std::to_string(copy.target) + ", which is not a position or a parameter", pc);
            const std::array<const RdnaOperand*, 4> sources{&instruction.source0, &instruction.source1, &instruction.source2, &instruction.source3};
            for (std::uint32_t component = 0; component < 4u; ++component) {
                if ((copy.enable & (1u << component)) == 0u) continue;
                const auto found = vectors.find(sources[component]->reg);
                if (found == vectors.end() || found->second.vertexOffset) FailCopy("an exported value other than a GSVS ring dword or a constant", pc);
                if (found->second.constant) {
                    copy.constants |= 1u << component;
                    copy.values[component] = found->second.value;
                } else {
                    copy.values[component] = *found->second.slot;
                }
            }
            for (const auto& previous : exports) {
                if (previous.target == copy.target) FailCopy("the export target " + std::to_string(copy.target) + " is written twice", pc);
            }
            exports.push_back(copy);
            continue;
        }
        Forget(vectors, scalars, instruction);
    }
    bool position = false;
    for (const auto& copy : exports) position = position || copy.target == 0x0cu;
    if (!position) throw std::runtime_error("geometry copy program: no position 0 export");
    return exports;
}

}
