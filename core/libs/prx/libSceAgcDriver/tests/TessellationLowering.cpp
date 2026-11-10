#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "Optimization/TessellationLowering.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <map>
#include <random>
#include <stdexcept>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t InputControlPoints = 3u;
constexpr std::uint32_t LocalStride = 32u;
constexpr std::uint32_t PatchStride = InputControlPoints * LocalStride;
constexpr std::uint32_t ReadPc = 0x40u;
constexpr std::uint32_t OutputControlPoints = 4u;
constexpr std::uint32_t ControlStride = 64u;
constexpr std::uint32_t ControlPatchStride = OutputControlPoints * ControlStride;
constexpr std::uint32_t RingMemory = 1u;
constexpr std::uint32_t RingWritePc = 0x80u;
constexpr std::uint32_t RingReadPc = 0xc0u;

bool IsKind(const IrValue* value, TessellationAttribute kind) {
    return value->HasImmediate() && value->Type() == IrType::U32 && value->ImmediateU32() == static_cast<std::uint32_t>(kind);
}

struct Hull {
    IrProgram program;
    IrBuilder builder{program};
    IrBlock* block = nullptr;

    Hull() {
        program.Resources().stage = IrShaderStage::TessellationControl;
        program.Resources().memoryInfo.push_back(MemoryInfo{.kind = ResourceKind::Lds});
        program.Resources().memoryInfo.push_back(MemoryInfo{.kind = ResourceKind::Buffer, .offen = true});
        static_cast<void>(newBlock());
    }

    IrBlock& newBlock() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        BlockInfo info;
        info.id = created.Id();
        program.Metadata().blockInfo.push_back(info);
        if (block == nullptr) program.SetEntryBlock(created);
        block = &created;
        builder.SetInsertionPoint(created);
        return created;
    }

    IrValue& invocation() {
        return builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::InvocationId)), &builder.Constant(0u)});
    }

    IrValue& relativePatch() {
        return builder.Emit(IrOpcode::TessellationBase, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(TessellationBaseKind::RelativeIndex))});
    }

    IrValue& computed(std::uint32_t limit, std::uint32_t multiple) {
        auto& index = builder.Emit(IrOpcode::UMin32, IrType::U32, {&invocation(), &builder.Constant(limit)});
        return builder.IMul(index, builder.Constant(multiple));
    }

    IrValue& firstInvocation() {
        return builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&invocation(), &builder.Constant(1u)});
    }

    IrValue& select(std::uint32_t firstLimit, std::uint32_t firstMultiple, IrValue& second) {
        return builder.Emit(IrOpcode::SelectU32, IrType::U32, {&firstInvocation(), &computed(firstLimit, firstMultiple), &second});
    }

    IrValue& inPatch(IrValue& offset) {
        return builder.IAdd(builder.IMul(relativePatch(), builder.Constant(PatchStride)), offset);
    }

    IrValue& read(IrValue& address) {
        auto& loaded = builder.Emit(IrOpcode::LoadSharedU32, IrType::U32, {&address, &builder.ConstantBool(true)}, MemoryFlags{0u, ReadPc});
        static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&loaded}));
        return loaded;
    }

    IrValue& read(IrValue& address, IrValue& predicate) {
        auto& loaded = builder.Emit(IrOpcode::LoadSharedU32, IrType::U32, {&address, &predicate}, MemoryFlags{0u, ReadPc});
        static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&loaded}));
        return loaded;
    }

    IrValue& complementedOwnLaneBit(IrValue& condition) {
        auto& ballot = builder.Emit(IrOpcode::Ballot, IrType::U32x4, {&condition});
        auto& word = builder.Emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &builder.Constant(0u)});
        auto& complement = builder.Emit(IrOpcode::BitwiseNot32, IrType::U32, {&word});
        auto& lane = builder.Emit(IrOpcode::LaneId, IrType::U32, {});
        auto& laneBit = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&lane, &builder.Constant(31u)});
        auto& shifted = builder.Emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&complement, &laneBit});
        auto& bit = builder.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&shifted, &builder.Constant(1u)});
        return builder.Emit(IrOpcode::INotEqual32, IrType::Bool, {&bit, &builder.Constant(0u)});
    }

    IrValue& ring(IrValue& offset) {
        return builder.IAdd(builder.IMul(relativePatch(), builder.Constant(ControlPatchStride)), offset);
    }

    IrValue& ownControlPoint(std::uint32_t field) {
        return ring(builder.IAdd(builder.IMul(invocation(), builder.Constant(ControlStride)), builder.Constant(field)));
    }

    IrValue& controlPoint(std::uint32_t point, std::uint32_t field) {
        return ring(builder.Constant(point * ControlStride + field));
    }

    IrValue& ringResource() {
        return builder.Emit(IrOpcode::GetBufferResource, IrType::BufferResource, {&builder.Constant(0u), &builder.Constant(0u), &builder.Constant(0u), &builder.Constant(0u)});
    }

    IrValue& offChip() {
        return builder.Emit(IrOpcode::TessellationBase, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(TessellationBaseKind::OffChip))});
    }

    void writeRing(IrValue& offset) {
        static_cast<void>(builder.Emit(IrOpcode::StoreBufferU32, IrType::Void, {&ringResource(), &builder.Constant(0u), &offset, &offChip(), &builder.Constant(1u), &builder.ConstantBool(true)}, MemoryFlags{RingMemory, RingWritePc}));
    }

    void readRing(IrValue& offset, IrValue& predicate) {
        auto& loaded = builder.Emit(IrOpcode::LoadBufferU32, IrType::U32, {&ringResource(), &builder.Constant(0u), &offset, &offChip(), &predicate}, MemoryFlags{RingMemory, RingReadPc});
        static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&loaded}));
    }

    IrValue& packed() {
        return builder.BitwiseOr(builder.ShiftLeftLogical(invocation(), builder.Constant(8u)), relativePatch());
    }

    IrValue& merge(std::uint32_t firstMultiple, std::uint32_t secondMultiple) {
        IrBlock& entry = *block;
        IrBlock& first = newBlock();
        IrBlock& second = newBlock();
        IrBlock& together = newBlock();
        builder.SetInsertionPoint(entry);
        builder.BranchConditional(firstInvocation(), first, second);
        builder.SetInsertionPoint(first);
        auto& left = computed(1u, firstMultiple);
        builder.Branch(together);
        builder.SetInsertionPoint(second);
        auto& right = computed(1u, secondMultiple);
        builder.Branch(together);
        builder.SetInsertionPoint(together);
        auto& merged = builder.Emit(IrOpcode::Phi, IrType::U32, {});
        merged.AddPhiOperand(&first, &left);
        merged.AddPhiOperand(&second, &right);
        return merged;
    }
};

struct Outcome {
    bool lowered = false;
    std::string reason;
    const IrValue* emitted = nullptr;
};

const ShaderTessellationInputInfo Tessellation{.inputControlPoints = InputControlPoints, .outputControlPoints = InputControlPoints, .patchesPerGroup = 1u};
const ShaderTessellationInputInfo RingTessellation{.inputControlPoints = InputControlPoints, .outputControlPoints = OutputControlPoints, .patchesPerGroup = 1u};
const ShaderTessellationInputInfo GroupedTessellation{.inputControlPoints = InputControlPoints, .outputControlPoints = InputControlPoints, .patchesPerGroup = 1u, .primitiveGroupSize = 1u};

Outcome Lower(IrProgram& program, const ShaderTessellationInputInfo& tessellation = Tessellation) {
    Outcome outcome;
    try {
        TessellationLowering{}.Lower(program, tessellation);
    } catch (const std::exception& error) {
        outcome.reason = error.what();
        return outcome;
    }
    outcome.lowered = true;
    for (IrBlock* block : program.BlockOrder()) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::GetTessellationAttribute && IsKind(inst->Argument(0), TessellationAttribute::ControlInput)) {
                outcome.emitted = inst->Argument(1)->Resolve();
            }
        }
    }
    return outcome;
}

bool LoweredAt(IrProgram& program, IrValue& address) {
    const Outcome outcome = Lower(program);
    if (!outcome.lowered) std::fprintf(stderr, "refused: %s\n", outcome.reason.c_str());
    return outcome.lowered && outcome.emitted == address.Resolve();
}

bool RefusedBecause(IrProgram& program, const char* reason) {
    const Outcome outcome = Lower(program);
    if (outcome.lowered) std::fprintf(stderr, "lowered instead of refusing: %s\n", reason);
    return !outcome.lowered && outcome.reason.find(reason) != std::string::npos;
}

bool LowersRing(IrProgram& program) {
    const Outcome outcome = Lower(program, RingTessellation);
    if (!outcome.lowered) std::fprintf(stderr, "refused: %s\n", outcome.reason.c_str());
    return outcome.lowered;
}

bool RingRefusedBecause(IrProgram& program, const char* reason) {
    const Outcome outcome = Lower(program, RingTessellation);
    if (outcome.lowered) std::fprintf(stderr, "lowered instead of refusing: %s\n", reason);
    if (!outcome.lowered && outcome.reason.find(reason) == std::string::npos) std::fprintf(stderr, "refused for another reason: %s\n", outcome.reason.c_str());
    return !outcome.lowered && outcome.reason.find(reason) != std::string::npos;
}

bool IsBarrier(const IrValue* inst) {
    return inst->Opcode() == IrOpcode::Barrier;
}

bool IsBranch(const IrValue* inst) {
    return inst->Opcode() == IrOpcode::Branch || inst->Opcode() == IrOpcode::BranchConditional;
}

bool IsControlOutputWrite(const IrValue* inst) {
    return inst->Opcode() == IrOpcode::SetTessellationAttribute && IsKind(inst->Argument(0), TessellationAttribute::ControlOutput);
}

bool IsControlOutputRead(const IrValue* inst) {
    return inst->Opcode() == IrOpcode::GetTessellationAttribute && IsKind(inst->Argument(0), TessellationAttribute::ControlOutput);
}

std::ptrdiff_t FirstPosition(const IrBlock& block, bool (*match)(const IrValue*)) {
    const auto& instructions = block.Instructions();
    const auto found = std::find_if(instructions.begin(), instructions.end(), match);
    return found == instructions.end() ? -1 : std::distance(instructions.begin(), found);
}

std::ptrdiff_t LastPosition(const IrBlock& block, bool (*match)(const IrValue*)) {
    const auto& instructions = block.Instructions();
    const auto found = std::find_if(instructions.rbegin(), instructions.rend(), match);
    return found == instructions.rend() ? -1 : std::distance(instructions.begin(), found.base()) - 1;
}

std::size_t Barriers(const IrProgram& program) {
    std::size_t count = 0;
    for (const IrBlock* block : program.BlockOrder()) count += static_cast<std::size_t>(std::count_if(block->Instructions().begin(), block->Instructions().end(), IsBarrier));
    return count;
}

std::uint32_t ReadAddress(const IrBlock& block) {
    for (const IrValue* inst : block.Instructions()) {
        if (!IsControlOutputRead(inst)) continue;
        const IrValue* address = inst->Argument(1)->Resolve();
        return address->HasImmediate() ? address->ImmediateU32() : ~0u;
    }
    return ~0u;
}

bool BarrierBetweenWritesAndBranch(const IrBlock& block) {
    const auto barrier = FirstPosition(block, IsBarrier);
    return barrier > LastPosition(block, IsControlOutputWrite) && barrier < FirstPosition(block, IsBranch);
}

void SkipHullProgram(Hull& hull, IrBlock& entry, std::uint32_t lanes, IrBlock& skip, IrBlock& body) {
    hull.builder.SetInsertionPoint(entry);
    auto& lane = hull.builder.Emit(IrOpcode::LaneId, IrType::U32, {});
    auto& outside = hull.builder.Emit(IrOpcode::LogicalNot, IrType::Bool, {&hull.builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &hull.builder.Constant(lanes)})});
    hull.builder.BranchConditional(outside, skip, body);
    constexpr std::uint32_t Renumbered = 100u;
    for (auto& info : hull.program.Metadata().blockInfo) info.id += Renumbered;
    auto& info = hull.program.Metadata().blockInfo.front();
    info.terminator.kind = TerminatorKind::ConditionalBranch;
    info.terminator.trueBlock = skip.Id() + Renumbered;
    info.terminator.falseBlock = body.Id() + Renumbered;
    info.condition = &outside;
}

std::uint32_t EmittedControlPoint(IrShaderStage stage, TessellationAttribute kind, std::uint32_t address) {
    constexpr std::uint32_t ControlStride = 16u;
    IrProgram program;
    IrBuilder builder{program};
    program.SetWaveSize(64);
    program.Resources().stage = stage;
    program.Resources().srtPlanComplete = true;
    program.Resources().resourceTrackingComplete = true;
    program.Metadata().shaderInfoComplete = true;
    program.Metadata().bindingLayoutComplete = true;
    program.Metadata().tessellationControlStride = ControlStride;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    BlockInfo info;
    info.id = block.Id();
    program.Metadata().blockInfo.push_back(info);
    builder.SetInsertionPoint(block);
    auto& read = builder.Emit(IrOpcode::GetTessellationAttribute, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(kind)), &builder.Constant(address), &builder.ConstantBool(true)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&read}));
    ShaderVertexInputInfo vertex;
    vertex.tess.inputControlPoints = InputControlPoints;
    vertex.tess.outputControlPoints = InputControlPoints;
    vertex.tess.domain = 1u;
    vertex.tess.partitioning = 2u;
    vertex.tess.outputTopology = 2u;
    ShaderStageInputInfo inputs;
    inputs.vertex = &vertex;
    CompiledBindingLayout bindings;
    bindings.layout = program.Metadata().bindings;
    SpirvTargetOptions target{};
    target.vulkanVersion = 0x00401000;
    target.spirvVersion = 0x00010300;
    target.subgroupSize = 64;
    const auto words = SpirvEmitter().Emit(program, inputs, bindings, target);
    std::unordered_map<std::uint32_t, std::uint32_t> constants;
    for (std::size_t at = 5; at < words.size(); at += words[at] >> 16u) {
        const auto count = words[at] >> 16u;
        if (count == 0u || at + count > words.size()) throw std::runtime_error("the tessellation program compiled to a malformed SPIR-V instruction");
        const auto opcode = words[at] & 0xffffu;
        if (opcode == spv::OpConstant && count == 4u) constants[words[at + 2u]] = words[at + 3u];
        if (opcode == spv::OpAccessChain && count == 7u) return constants.at(words[at + 4u]);
    }
    throw std::runtime_error("the tessellation program reads no control point attribute");
}

std::uint32_t EmittedEvaluationControlPoint(std::uint32_t address) {
    return EmittedControlPoint(IrShaderStage::TessellationEvaluation, TessellationAttribute::EvaluationInput, address);
}

enum class Conjunction { None, Before, After };

bool LowersUnderComplementedBallot(Conjunction conjunction) {
    Hull hull;
    hull.program.SetWaveSize(32);
    auto& first = hull.firstInvocation();
    auto& notFirst = hull.builder.Emit(IrOpcode::LogicalNot, IrType::Bool, {&first});
    auto& own = hull.complementedOwnLaneBit(notFirst);
    auto& other = hull.builder.Emit(IrOpcode::ULessThan32, IrType::Bool, {&hull.invocation(), &hull.builder.Constant(2u)});
    IrValue* predicate = &own;
    if (conjunction == Conjunction::Before) predicate = &hull.builder.Emit(IrOpcode::LogicalAnd, IrType::Bool, {&own, &other});
    if (conjunction == Conjunction::After) predicate = &hull.builder.Emit(IrOpcode::LogicalAnd, IrType::Bool, {&other, &own});
    auto& square = hull.builder.IMul(hull.invocation(), hull.invocation());
    auto& offset = hull.builder.Emit(IrOpcode::SelectU32, IrType::U32, {&notFirst, &square, &hull.computed(2u, LocalStride)});
    auto& address = hull.inPatch(offset);
    hull.read(address, *predicate);
    return LoweredAt(hull.program, address);
}

bool OwnControlPoint(const IrValue* address, std::uint32_t stride) {
    if (address == nullptr || address->Opcode() != IrOpcode::IAdd32 || !address->Argument(1)->Resolve()->HasImmediate() || address->Argument(1)->Resolve()->ImmediateU32() != 0u) return false;
    const IrValue* scaled = address->Argument(0)->Resolve();
    if (scaled->Opcode() != IrOpcode::IMul32 || scaled->Argument(1)->Resolve()->ImmediateU32() != stride) return false;
    const IrValue* invocation = scaled->Argument(0)->Resolve();
    return invocation->Opcode() == IrOpcode::GetBuiltin && invocation->Argument(0)->Resolve()->ImmediateU32() == static_cast<std::uint32_t>(StageInputKind::InvocationId);
}

enum class Walk { Exact, StoreInBlock, NeighbourRead, UnresetValue, CountMask, ExitOnMask };

constexpr std::uint32_t RectCountOffset = 0u;
constexpr std::uint32_t ValidLanesOffset = 4u;
constexpr std::uint32_t RectBase = 0x100u;
constexpr std::uint32_t PointBase = 0x1000u;
constexpr std::uint32_t TableBase = 0x2000u;
constexpr std::uint32_t MemoryWords = (TableBase + 64u * 4u) / 4u;

struct QuadWalk {
    IrProgram program;
    IrBuilder builder{program};
    IrValue* wqm = nullptr;
    IrValue* ownBit = nullptr;
    IrValue* predicate = nullptr;
    IrValue* helperChoice = nullptr;
    IrValue* blockExec = nullptr;
    IrValue* strict = nullptr;

    explicit QuadWalk(Walk walk) {
        program.SetWaveSize(64);
        program.Resources().stage = IrShaderStage::TessellationControl;
        program.Resources().memoryInfo.push_back(MemoryInfo{.kind = ResourceKind::Lds});
        IrBlock& entry = block();
        IrBlock& head = block();
        IrBlock& test = block();
        IrBlock& sample = block();
        IrBlock& join = block();
        IrBlock& latch = block();
        IrBlock& exit = block();
        program.SetEntryBlock(entry);
        auto& b = builder;
        const auto u = [&](std::uint32_t value) -> IrValue& { return b.Constant(value); };
        const auto op = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) -> IrValue& { return b.Emit(opcode, type, arguments); };
        const auto select = [&](IrValue& condition, IrValue& yes, IrValue& no) -> IrValue& { return op(IrOpcode::SelectU32, IrType::U32, {&condition, &yes, &no}); };
        const auto both = [&](IrValue& lhs, IrValue& rhs) -> IrValue& { return op(IrOpcode::LogicalAnd, IrType::Bool, {&lhs, &rhs}); };
        const auto equal = [&](IrValue& lhs, IrValue& rhs) -> IrValue& { return op(IrOpcode::IEqual32, IrType::Bool, {&lhs, &rhs}); };
        const auto clear = [&](IrValue& value) -> IrValue& { return op(IrOpcode::LogicalNot, IrType::Bool, {&value}); };

        b.SetInsertionPoint(entry);
        IrValue& user = b.GetUserData(static_cast<ScalarReg>(0));
        IrValue& resource = op(IrOpcode::GetBufferResource, IrType::BufferResource, {&user, &user, &user, &user});
        IrValue& output = op(IrOpcode::GetAddressResource, IrType::AddressResource, {&user, &user});
        const auto read = [&](IrValue& offset) -> IrValue& { return op(IrOpcode::ReadConstBuffer, IrType::U32, {&resource, &offset}); };
        IrValue& lane = op(IrOpcode::LaneId, IrType::U32, {});
        IrValue& x = read(b.IAdd(b.IMul(lane, u(8u)), u(PointBase)));
        IrValue& y = read(b.IAdd(b.IMul(lane, u(8u)), u(PointBase + 4u)));
        IrValue& count = read(u(RectCountOffset));
        IrValue& entryExec = op(IrOpcode::ULessThan32, IrType::Bool, {&lane, &read(u(ValidLanesOffset))});
        jump(entry, head);

        b.SetInsertionPoint(head);
        IrValue& index = phi(IrType::U32);
        IrValue& active = phi(IrType::Bool);
        IrValue& v4 = phi(IrType::U32);
        IrValue& v13 = phi(IrType::U32);
        IrValue* v5 = walk == Walk::UnresetValue ? &phi(IrType::U32) : nullptr;
        IrValue& v4Reset = select(active, u(0u), v4);
        IrValue& exec = both(active, op(IrOpcode::ULessThan32, IrType::Bool, {&index, &count}));
        branch(head, clear(exec), BranchCondition::ExecZero, exit, test);

        b.SetInsertionPoint(test);
        IrValue& base = b.IAdd(b.IMul(index, u(16u)), u(RectBase));
        IrValue& lowX = read(base);
        IrValue& lowY = read(b.IAdd(base, u(4u)));
        IrValue& highX = read(b.IAdd(base, u(8u)));
        IrValue& highY = read(b.IAdd(base, u(12u)));
        const auto inside = [&](IrValue& mask) -> IrValue& {
            IrValue& left = both(mask, op(IrOpcode::ULessThanEqual32, IrType::Bool, {&lowX, &x}));
            IrValue& bottom = both(mask, op(IrOpcode::ULessThanEqual32, IrType::Bool, {&lowY, &y}));
            IrValue& right = both(mask, op(IrOpcode::UGreaterThanEqual32, IrType::Bool, {&highX, &x}));
            IrValue& top = both(mask, op(IrOpcode::UGreaterThanEqual32, IrType::Bool, {&highY, &y}));
            return both(both(both(left, bottom), right), top);
        };
        predicate = &inside(exec);
        IrValue& ballot = op(IrOpcode::Ballot, IrType::U32x4, {predicate});
        IrValue& words = b.ConstructU64(op(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &u(0u)}), op(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &u(1u)}));
        wqm = &op(IrOpcode::WqmU64, IrType::U64, {&words});
        IrValue& low = op(IrOpcode::CompositeExtractU64, IrType::U32, {wqm, &u(0u)});
        IrValue& high = op(IrOpcode::CompositeExtractU64, IrType::U32, {wqm, &u(1u)});
        IrValue& bitLane = op(IrOpcode::LaneId, IrType::U32, {});
        IrValue& word = select(op(IrOpcode::ULessThan32, IrType::Bool, {&bitLane, &u(32u)}), low, high);
        IrValue& shifted = b.ShiftRightLogical(word, b.BitwiseAnd(bitLane, u(31u)));
        ownBit = &op(IrOpcode::INotEqual32, IrType::Bool, {&b.BitwiseAnd(shifted, u(1u)), &u(0u)});
        if (walk == Walk::CountMask) static_cast<void>(op(IrOpcode::ReferenceU32, IrType::Void, {&op(IrOpcode::BitCount32, IrType::U32, {&low})}));
        helperChoice = &select(*ownBit, u(2u), u(3u));
        IrValue& v13Test = select(exec, *helperChoice, v13);
        blockExec = &both(exec, equal(u(2u), v13Test));
        branch(test, clear(*blockExec), BranchCondition::ExecZero, join, sample);

        b.SetInsertionPoint(sample);
        strict = &inside(*blockExec);
        IrValue& fetched = read(b.IAdd(b.IMul(b.BitwiseAnd(x, u(63u)), u(4u)), u(TableBase)));
        IrValue* value = &b.IAdd(fetched, b.IMul(index, u(1000u)));
        if (walk == Walk::NeighbourRead) value = &b.IAdd(*value, op(IrOpcode::DppMoveU32, IrType::U32, {&v13Test, &b.ConstantBool(true)}));
        if (walk == Walk::StoreInBlock) static_cast<void>(b.Emit(IrOpcode::WriteSharedU32, IrType::Void, {&b.IMul(lane, u(4u)), value, blockExec}, std::bit_cast<std::uint64_t>(MemoryFlags{0u, 0x200u})));
        IrValue& v4New = select(*blockExec, *value, v4Reset);
        IrValue& v13New = select(*blockExec, select(walk == Walk::ExitOnMask ? *ownBit : *strict, u(4u), u(3u)), v13Test);
        IrValue* v5New = v5 != nullptr ? &select(*blockExec, *value, *v5) : nullptr;
        jump(sample, join);

        b.SetInsertionPoint(join);
        IrValue& v4Out = phi(IrType::U32);
        IrValue& v13Out = phi(IrType::U32);
        IrValue* v5Out = v5 != nullptr ? &phi(IrType::U32) : nullptr;
        v4Out.AddPhiOperand(&test, &v4Reset);
        v4Out.AddPhiOperand(&sample, &v4New);
        v13Out.AddPhiOperand(&test, &v13Test);
        v13Out.AddPhiOperand(&sample, &v13New);
        if (v5Out != nullptr) {
            v5Out->AddPhiOperand(&test, v5);
            v5Out->AddPhiOperand(&sample, v5New);
        }
        IrValue& more = both(entryExec, equal(u(3u), v13Out));
        IrValue& v13Next = select(more, u(0u), v13Out);
        IrValue& next = b.IAdd(index, u(1u));
        branch(join, clear(more), BranchCondition::ExecZero, exit, latch);
        jump(latch, head);

        index.AddPhiOperand(&entry, &u(0u));
        index.AddPhiOperand(&latch, &next);
        active.AddPhiOperand(&entry, &entryExec);
        active.AddPhiOperand(&latch, &more);
        v4.AddPhiOperand(&entry, &u(0u));
        v4.AddPhiOperand(&latch, &v4Out);
        v13.AddPhiOperand(&entry, &u(0u));
        v13.AddPhiOperand(&latch, &v13Next);
        if (v5 != nullptr) {
            v5->AddPhiOperand(&entry, &u(0u));
            v5->AddPhiOperand(&latch, v5Out);
        }

        b.SetInsertionPoint(exit);
        IrValue& v4Exit = phi(IrType::U32);
        IrValue& v13Exit = phi(IrType::U32);
        IrValue* v5Exit = v5 != nullptr ? &phi(IrType::U32) : nullptr;
        v4Exit.AddPhiOperand(&head, &v4Reset);
        v4Exit.AddPhiOperand(&join, &v4Out);
        v13Exit.AddPhiOperand(&head, &v13);
        v13Exit.AddPhiOperand(&join, &v13Out);
        if (v5Exit != nullptr) {
            v5Exit->AddPhiOperand(&head, v5);
            v5Exit->AddPhiOperand(&join, v5Out);
        }
        const auto store = [&](std::uint32_t slot, IrValue& data) {
            static_cast<void>(op(IrOpcode::StoreAddressU32, IrType::Void, {&output, &b.IAdd(b.IMul(lane, u(16u)), u(slot)), &u(0u), &data, &entryExec}));
        };
        store(0u, v4Exit);
        store(4u, v13Exit);
        if (v5Exit != nullptr) store(8u, *v5Exit);
    }

    IrBlock& block() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        BlockInfo info;
        info.id = created.Id();
        program.Metadata().blockInfo.push_back(info);
        return created;
    }

    IrValue& phi(IrType type) {
        return builder.Emit(IrOpcode::Phi, type, {});
    }

    BlockInfo& info(const IrBlock& block) {
        auto& order = program.BlockOrder();
        for (std::size_t index = 0; index < order.size(); ++index) {
            if (order[index] == &block) return program.Metadata().blockInfo[index];
        }
        throw std::runtime_error("block outside the program");
    }

    void jump(IrBlock& from, IrBlock& to) {
        auto& terminator = info(from).terminator;
        terminator.kind = TerminatorKind::Branch;
        terminator.trueBlock = to.Id();
        from.AddBranch(&to);
    }

    void branch(IrBlock& from, IrValue& condition, BranchCondition kind, IrBlock& taken, IrBlock& fallthrough) {
        builder.SetInsertionPoint(from);
        static_cast<void>(builder.Emit(IrOpcode::Reference, IrType::Void, {&condition}));
        auto& blockInfo = info(from);
        blockInfo.terminator.kind = TerminatorKind::ConditionalBranch;
        blockInfo.terminator.condition = kind;
        blockInfo.terminator.trueBlock = taken.Id();
        blockInfo.terminator.falseBlock = fallthrough.Id();
        blockInfo.condition = &condition;
        from.AddBranch(&taken);
        from.AddBranch(&fallthrough);
    }
};

bool Contains(const IrProgram& program, IrOpcode opcode) {
    for (const IrBlock* block : program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == opcode) return true;
        }
    }
    return false;
}

bool QuadRefusedBecause(Walk walk, std::initializer_list<const char*> reasons) {
    QuadWalk hull(walk);
    const Outcome outcome = Lower(hull.program);
    if (outcome.lowered) {
        std::fprintf(stderr, "lowered instead of refusing: %s\n", *reasons.begin());
        return false;
    }
    const bool quad = outcome.reason.find("whole-quad lane mask") != std::string::npos;
    const bool because = std::any_of(reasons.begin(), reasons.end(), [&](const char* reason) { return outcome.reason.find(reason) != std::string::npos; });
    if (!quad || !because) std::fprintf(stderr, "refused for another reason: %s\n", outcome.reason.c_str());
    return quad && because;
}

using Lanes = std::array<std::uint64_t, 64>;

struct WaveRun {
    std::map<std::uint32_t, std::uint32_t> stores;
    std::size_t helpers = 0;
};

std::uint64_t LaneValue(const IrValue* raw, const std::unordered_map<const IrValue*, Lanes>& values, std::uint32_t lane) {
    const IrValue* value = raw->Resolve();
    if (value->HasImmediate()) return value->Type() == IrType::Bool ? (value->ImmediateBool() ? 1u : 0u) : value->ImmediateU64();
    return values.at(value)[lane];
}

WaveRun RunWave(const IrProgram& program, const std::vector<std::uint32_t>& memory, const std::vector<std::uint32_t>& group, const IrValue* blockExec = nullptr, const IrValue* strict = nullptr) {
    constexpr std::uint64_t Word = 0xffffffffu;
    WaveRun run;
    std::unordered_map<const IrValue*, Lanes> values;
    const auto& order = program.BlockOrder();
    const auto byId = [&](std::uint32_t id) -> IrBlock* {
        for (std::size_t index = 0; index < order.size(); ++index) {
            if (program.Metadata().blockInfo[index].id == id) return order[index];
        }
        return nullptr;
    };
    IrBlock* block = &program.EntryBlock();
    IrBlock* from = nullptr;
    for (std::size_t steps = 0; block != nullptr; ++steps) {
        if (steps > 10000u) throw std::runtime_error("the wave does not leave its loop");
        std::vector<std::pair<const IrValue*, Lanes>> incoming;
        for (const IrValue* inst : block->Instructions()) {
            if (inst->IsIdentity()) continue;
            if (!inst->IsPhi()) break;
            Lanes lanes{};
            bool found = false;
            for (std::size_t operand = 0; operand < inst->ArgumentCount(); ++operand) {
                if (inst->PhiBlock(operand) != from) continue;
                for (const std::uint32_t lane : group) lanes[lane] = LaneValue(inst->Argument(operand), values, lane);
                found = true;
                break;
            }
            if (!found) throw std::runtime_error("a phi has no value for the edge the wave took");
            incoming.emplace_back(inst, lanes);
        }
        for (const auto& [phi, lanes] : incoming) values[phi] = lanes;
        for (const IrValue* inst : block->Instructions()) {
            if (inst->IsIdentity() || inst->IsPhi()) continue;
            const auto arg = [&](std::size_t index, std::uint32_t lane) { return LaneValue(inst->Argument(index), values, lane); };
            Lanes result{};
            switch (inst->Opcode()) {
            case IrOpcode::Reference:
            case IrOpcode::ReferenceU32:
                continue;
            case IrOpcode::StoreAddressU32:
                for (const std::uint32_t lane : group) {
                    if (arg(4, lane) != 0u) run.stores[static_cast<std::uint32_t>(arg(1, lane))] = static_cast<std::uint32_t>(arg(3, lane));
                }
                continue;
            case IrOpcode::Ballot: {
                std::uint64_t mask = 0;
                for (const std::uint32_t lane : group) mask |= arg(0, lane) != 0u ? std::uint64_t{1} << lane : 0u;
                for (const std::uint32_t lane : group) result[lane] = mask;
                break;
            }
            default:
                for (const std::uint32_t lane : group) {
                    const auto a = [&](std::size_t index) { return arg(index, lane); };
                    std::uint64_t value = 0;
                    switch (inst->Opcode()) {
                    case IrOpcode::GetUserData:
                    case IrOpcode::GetBufferResource:
                    case IrOpcode::GetAddressResource:
                        value = 0;
                        break;
                    case IrOpcode::LaneId: value = lane; break;
                    case IrOpcode::ReadConstBuffer: value = a(1) / 4u < memory.size() ? memory[a(1) / 4u] : 0u; break;
                    case IrOpcode::IAdd32: value = (a(0) + a(1)) & Word; break;
                    case IrOpcode::IMul32: value = (a(0) * a(1)) & Word; break;
                    case IrOpcode::BitwiseAnd32: value = a(0) & a(1) & Word; break;
                    case IrOpcode::BitwiseOr32: value = (a(0) | a(1)) & Word; break;
                    case IrOpcode::ShiftRightLogical32: value = (a(0) & Word) >> (a(1) & 31u); break;
                    case IrOpcode::BitCount32: value = static_cast<std::uint64_t>(std::popcount(a(0) & Word)); break;
                    case IrOpcode::ULessThan32: value = (a(0) & Word) < (a(1) & Word); break;
                    case IrOpcode::ULessThanEqual32: value = (a(0) & Word) <= (a(1) & Word); break;
                    case IrOpcode::UGreaterThanEqual32: value = (a(0) & Word) >= (a(1) & Word); break;
                    case IrOpcode::IEqual32: value = (a(0) & Word) == (a(1) & Word); break;
                    case IrOpcode::INotEqual32: value = (a(0) & Word) != (a(1) & Word); break;
                    case IrOpcode::LogicalAnd: value = a(0) != 0u && a(1) != 0u; break;
                    case IrOpcode::LogicalOr: value = a(0) != 0u || a(1) != 0u; break;
                    case IrOpcode::LogicalXor: value = (a(0) != 0u) != (a(1) != 0u); break;
                    case IrOpcode::LogicalNot: value = a(0) == 0u; break;
                    case IrOpcode::SelectU32:
                    case IrOpcode::SelectU1: value = a(0) != 0u ? a(1) : a(2); break;
                    case IrOpcode::CompositeExtractU32x4:
                    case IrOpcode::CompositeExtractU64: value = a(1) < 2u ? (a(0) >> (32u * a(1))) & Word : 0u; break;
                    case IrOpcode::CompositeConstructU64: value = (a(0) & Word) | (a(1) << 32u); break;
                    case IrOpcode::WqmU64:
                        for (std::uint32_t quad = 0; quad < 16u; ++quad) value |= ((a(0) >> (4u * quad)) & 0xfu) != 0u ? std::uint64_t{0xf} << (4u * quad) : 0u;
                        break;
                    default:
                        throw std::runtime_error("the CPU model has no " + std::string(IrOpcodeName(inst->Opcode())));
                    }
                    result[lane] = value;
                }
                break;
            }
            values[inst] = result;
            if (inst == strict && blockExec != nullptr) {
                for (const std::uint32_t lane : group) run.helpers += values.at(blockExec)[lane] != 0u && result[lane] == 0u;
            }
        }
        const auto& info = program.Metadata().blockInfo.at(static_cast<std::size_t>(std::find(order.begin(), order.end(), block) - order.begin()));
        IrBlock* next = nullptr;
        if (info.terminator.kind == TerminatorKind::ConditionalBranch && info.condition != nullptr) {
            bool all = true;
            bool any = false;
            for (const std::uint32_t lane : group) {
                const bool set = LaneValue(info.condition, values, lane) != 0u;
                all = all && set;
                any = any || set;
            }
            const auto kind = info.terminator.condition;
            const bool taken = kind == BranchCondition::ExecZero || kind == BranchCondition::VccZero ? all : any;
            next = byId(taken ? info.terminator.trueBlock : info.terminator.falseBlock);
        } else if (info.terminator.kind == TerminatorKind::Branch) {
            next = byId(info.terminator.trueBlock);
        }
        from = block;
        block = next;
    }
    return run;
}

struct ModelResult {
    std::size_t helpers = 0;
    std::size_t mismatches = 0;
    std::size_t compared = 0;
};

ModelResult CompareWithConsole(const QuadWalk& console, const IrProgram& lowered, std::uint32_t lists) {
    constexpr std::uint32_t Patches = 85u;
    constexpr std::uint32_t ControlPoints = 3u;
    constexpr std::uint32_t Threads = Patches * ControlPoints;
    std::mt19937 random(0x5eedu);
    ModelResult result;
    for (std::uint32_t list = 0; list < lists; ++list) {
        std::vector<std::uint32_t> points(2u * Threads);
        for (std::uint32_t patch = 0; patch < Patches; ++patch) {
            const std::uint32_t centerX = random() % 32u;
            const std::uint32_t centerY = random() % 32u;
            for (std::uint32_t point = 0; point < ControlPoints; ++point) {
                points[2u * (patch * ControlPoints + point)] = centerX + random() % 5u;
                points[2u * (patch * ControlPoints + point) + 1u] = centerY + random() % 5u;
            }
        }
        const std::uint32_t rectangles = list == 0u ? 0u : random() % 6u;
        std::vector<std::uint32_t> rectangleWords;
        for (std::uint32_t rectangle = 0; rectangle < rectangles; ++rectangle) {
            const std::uint32_t lowX = random() % 28u;
            const std::uint32_t lowY = random() % 28u;
            rectangleWords.insert(rectangleWords.end(), {lowX, lowY, lowX + random() % 10u, lowY + random() % 10u});
        }
        std::vector<std::uint32_t> table(64u);
        for (auto& entry : table) entry = random();
        for (std::uint32_t wave = 0; wave * 64u < Threads; ++wave) {
            std::vector<std::uint32_t> memory(MemoryWords);
            const std::uint32_t valid = std::min(64u, Threads - wave * 64u);
            memory[RectCountOffset / 4u] = rectangles;
            memory[ValidLanesOffset / 4u] = valid;
            std::copy(rectangleWords.begin(), rectangleWords.end(), memory.begin() + RectBase / 4u);
            for (std::uint32_t lane = 0; lane < valid; ++lane) {
                memory[(PointBase + 8u * lane) / 4u] = points[2u * (wave * 64u + lane)];
                memory[(PointBase + 8u * lane + 4u) / 4u] = points[2u * (wave * 64u + lane) + 1u];
            }
            std::copy(table.begin(), table.end(), memory.begin() + TableBase / 4u);
            std::vector<std::uint32_t> wave64(64u);
            for (std::uint32_t lane = 0; lane < 64u; ++lane) wave64[lane] = lane;
            const WaveRun reference = RunWave(console.program, memory, wave64, console.blockExec, console.strict);
            result.helpers += reference.helpers;
            std::vector<std::uint32_t> subgroup;
            for (std::uint32_t lane = 0; lane < valid; ++lane) {
                subgroup.push_back(lane);
                const WaveRun alone = RunWave(lowered, memory, {lane});
                for (std::uint32_t slot = 0; slot < 12u; slot += 4u) {
                    const auto expected = reference.stores.find(lane * 16u + slot);
                    const auto actual = alone.stores.find(lane * 16u + slot);
                    if (expected == reference.stores.end() && actual == alone.stores.end()) continue;
                    ++result.compared;
                    if (expected == reference.stores.end() || actual == alone.stores.end() || expected->second != actual->second) ++result.mismatches;
                }
            }
            if (RunWave(lowered, memory, subgroup).stores != reference.stores) ++result.mismatches;
        }
    }
    return result;
}

bool run(const char* name, const std::function<bool()>& body) {
    bool passed = false;
    try {
        passed = body();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", name, error.what());
    }
    if (!passed) std::fprintf(stderr, "failed: %s\n", name);
    return passed;
}

}

int main() {
    bool passed = true;

    passed &= run("a hull read at a computed control-point index keeps the program's address", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.computed(2u, LocalStride));
        hull.read(address);
        return LoweredAt(hull.program, address);
    });

    passed &= run("a hull read at a control point selected from data keeps the program's address", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.select(2u, LocalStride, hull.computed(1u, LocalStride)));
        hull.read(address);
        return LoweredAt(hull.program, address);
    });

    passed &= run("a hull read at a control point merged by a phi keeps the program's address", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.merge(LocalStride, LocalStride));
        hull.read(address);
        return LoweredAt(hull.program, address);
    });

    passed &= run("a selected control point at half a slot is refused", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.select(2u, LocalStride, hull.computed(1u, LocalStride / 2u)));
        hull.read(address);
        return RefusedBecause(hull.program, "not a whole number of control points");
    });

    passed &= run("a control point merged by a phi at half a slot is refused", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.merge(LocalStride, LocalStride / 2u));
        hull.read(address);
        return RefusedBecause(hull.program, "not a whole number of control points");
    });

    passed &= run("a selected control point at two different offsets is refused", [] {
        Hull hull;
        auto& offset = hull.builder.IAdd(hull.computed(1u, LocalStride), hull.builder.Constant(LocalStride));
        auto& address = hull.inPatch(hull.select(2u, LocalStride, offset));
        hull.read(address);
        return RefusedBecause(hull.program, "not a whole number of control points");
    });

    passed &= run("a hull read at a computed control-point index stays in its patch when the relative patch is also data", [] {
        Hull hull;
        auto& address = hull.inPatch(hull.computed(2u, LocalStride));
        hull.read(address);
        static_cast<void>(hull.builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&hull.relativePatch()}));
        const Outcome outcome = Lower(hull.program, GroupedTessellation);
        if (!outcome.lowered) std::fprintf(stderr, "refused: %s\n", outcome.reason.c_str());
        if (!outcome.lowered || outcome.emitted->Opcode() != IrOpcode::ISub32 || outcome.emitted->Argument(0)->Resolve() != address.Resolve()) return false;
        const IrValue* patch = outcome.emitted->Argument(1)->Resolve();
        return patch->Opcode() == IrOpcode::IMul32 && patch->Argument(1)->Resolve()->ImmediateU32() == PatchStride;
    });

    passed &= run("a hull read under its own lane's bit of a complemented ballot takes the select arm that bit implies", [] {
        return LowersUnderComplementedBallot(Conjunction::None);
    });

    passed &= run("a hull read under that bit and another condition takes the same select arm", [] {
        return LowersUnderComplementedBallot(Conjunction::Before);
    });

    passed &= run("a hull read under another condition and that bit takes the same select arm", [] {
        return LowersUnderComplementedBallot(Conjunction::After);
    });

    passed &= run("a domain read at an immediate address past the last control point reads the last control point", [] {
        return EmittedEvaluationControlPoint(InputControlPoints * 16u) == InputControlPoints - 1u;
    });

    passed &= run("a hull read of another control point's off-chip output behind a branch gets a barrier after every control point's writes", [] {
        Hull hull;
        hull.writeRing(hull.ownControlPoint(0u));
        hull.writeRing(hull.ownControlPoint(16u));
        IrBlock& entry = *hull.block;
        IrBlock& reader = hull.newBlock();
        IrBlock& done = hull.newBlock();
        hull.builder.SetInsertionPoint(entry);
        hull.builder.BranchConditional(hull.firstInvocation(), reader, done);
        hull.builder.SetInsertionPoint(reader);
        hull.readRing(hull.controlPoint(1u, 16u), hull.firstInvocation());
        hull.builder.Branch(done);
        return LowersRing(hull.program) && Barriers(hull.program) == 1u && BarrierBetweenWritesAndBranch(entry) && ReadAddress(reader) == ControlStride + 16u;
    });

    passed &= run("a hull read of another control point's off-chip output in the block that writes it gets a barrier between them", [] {
        Hull hull;
        hull.writeRing(hull.ownControlPoint(0u));
        hull.readRing(hull.controlPoint(2u, 0u), hull.builder.ConstantBool(true));
        if (!LowersRing(hull.program) || Barriers(hull.program) != 1u) return false;
        const auto barrier = FirstPosition(*hull.block, IsBarrier);
        return barrier > LastPosition(*hull.block, IsControlOutputWrite) && barrier < FirstPosition(*hull.block, IsControlOutputRead) && ReadAddress(*hull.block) == 2u * ControlStride;
    });

    passed &= run("a hull read of its own control point's off-chip output needs no barrier", [] {
        Hull hull;
        hull.writeRing(hull.ownControlPoint(0u));
        hull.readRing(hull.ownControlPoint(0u), hull.builder.ConstantBool(true));
        if (!LowersRing(hull.program) || Barriers(hull.program) != 0u) return false;
        const auto read = FirstPosition(*hull.block, IsControlOutputRead);
        if (read < 0) return false;
        const IrValue* address = (*std::next(hull.block->Instructions().begin(), read))->Argument(1)->Resolve();
        if (address->Opcode() != IrOpcode::IAdd32) return false;
        const IrValue* scaled = address->Argument(0)->Resolve();
        return scaled->Opcode() == IrOpcode::IMul32 && scaled->Argument(1)->Resolve()->ImmediateU32() == ControlStride;
    });

    passed &= run("a branch around the hull program that no control point takes does not keep the barrier from its writes", [] {
        Hull hull;
        IrBlock& entry = *hull.block;
        IrBlock& body = hull.newBlock();
        hull.writeRing(hull.ownControlPoint(0u));
        IrBlock& reader = hull.newBlock();
        hull.readRing(hull.controlPoint(1u, 0u), hull.firstInvocation());
        IrBlock& done = hull.newBlock();
        SkipHullProgram(hull, entry, OutputControlPoints, done, body);
        hull.builder.SetInsertionPoint(body);
        hull.builder.BranchConditional(hull.firstInvocation(), reader, done);
        hull.builder.SetInsertionPoint(reader);
        hull.builder.Branch(done);
        return LowersRing(hull.program) && Barriers(hull.program) == 1u && BarrierBetweenWritesAndBranch(body);
    });

    passed &= run("a branch around the hull program that some control points take is refused", [] {
        Hull hull;
        IrBlock& entry = *hull.block;
        IrBlock& body = hull.newBlock();
        hull.writeRing(hull.ownControlPoint(0u));
        IrBlock& reader = hull.newBlock();
        hull.readRing(hull.controlPoint(1u, 0u), hull.firstInvocation());
        IrBlock& done = hull.newBlock();
        SkipHullProgram(hull, entry, OutputControlPoints - 1u, done, body);
        hull.builder.SetInsertionPoint(body);
        hull.builder.BranchConditional(hull.firstInvocation(), reader, done);
        hull.builder.SetInsertionPoint(reader);
        hull.builder.Branch(done);
        return RingRefusedBecause(hull.program, "one of its writes there comes after the last block every invocation reaches before the read");
    });

    passed &= run("a hull read of another control point's off-chip output that no control point reaches is refused", [] {
        Hull hull;
        IrBlock& entry = *hull.block;
        IrBlock& body = hull.newBlock();
        hull.writeRing(hull.ownControlPoint(0u));
        hull.readRing(hull.controlPoint(1u, 0u), hull.builder.ConstantBool(true));
        IrBlock& done = hull.newBlock();
        SkipHullProgram(hull, entry, 0u, done, body);
        hull.builder.SetInsertionPoint(body);
        hull.builder.Branch(done);
        return RingRefusedBecause(hull.program, "in a block no control point reaches");
    });

    passed &= run("a hull write of its off-chip output after another control point's read is refused", [] {
        Hull hull;
        hull.writeRing(hull.ownControlPoint(0u));
        hull.readRing(hull.controlPoint(1u, 0u), hull.builder.ConstantBool(true));
        hull.writeRing(hull.ownControlPoint(16u));
        return RingRefusedBecause(hull.program, "one of its writes there comes after the last block every invocation reaches before the read");
    });

    passed &= run("a hull read of another control point's off-chip output in a loop through the program's entry is refused", [] {
        Hull hull;
        IrBlock& entry = *hull.block;
        hull.writeRing(hull.ownControlPoint(0u));
        hull.readRing(hull.controlPoint(1u, 0u), hull.builder.ConstantBool(true));
        IrBlock& done = hull.newBlock();
        hull.builder.SetInsertionPoint(entry);
        hull.builder.BranchConditional(hull.firstInvocation(), entry, done);
        return RingRefusedBecause(hull.program, "no block before the read is reached once by every invocation");
    });

    passed &= run("a hull read of off-chip memory where no control point writes is refused", [] {
        Hull hull;
        hull.readRing(hull.controlPoint(1u, 0u), hull.builder.ConstantBool(true));
        return RingRefusedBecause(hull.program, "writes no control point there");
    });

    passed &= run("a hull read past its patch's off-chip control points is refused", [] {
        Hull hull;
        hull.writeRing(hull.ownControlPoint(0u));
        hull.readRing(hull.controlPoint(OutputControlPoints, 0u), hull.builder.ConstantBool(true));
        return RingRefusedBecause(hull.program, "reads off-chip memory outside its patch's control points");
    });

    passed &= run("a hull read of another control point's output at an immediate address reads that control point", [] {
        return EmittedControlPoint(IrShaderStage::TessellationControl, TessellationAttribute::ControlOutput, 2u * 16u + 4u) == 2u;
    });

    passed &= run("a hull read of another control point's output at an immediate address past the last control point reads the last control point", [] {
        return EmittedControlPoint(IrShaderStage::TessellationControl, TessellationAttribute::ControlOutput, InputControlPoints * 16u) == InputControlPoints - 1u;
    });

    passed &= run("a hull read at its control point masked out of the packed control point and relative patch reads its own input control point", [] {
        Hull hull;
        auto& masked = hull.builder.BitwiseAnd(hull.packed(), hull.builder.Constant(0x1f00u));
        hull.read(hull.inPatch(hull.builder.ShiftRightLogical(masked, hull.builder.Constant(3u))));
        const Outcome outcome = Lower(hull.program);
        if (!outcome.lowered) std::fprintf(stderr, "refused: %s\n", outcome.reason.c_str());
        return outcome.lowered && OwnControlPoint(outcome.emitted, LocalStride);
    });

    passed &= run("a mask that drops the low bit of the packed control point is refused", [] {
        Hull hull;
        auto& masked = hull.builder.BitwiseAnd(hull.packed(), hull.builder.Constant(0x1e00u));
        hull.read(hull.inPatch(hull.builder.ShiftRightLogical(masked, hull.builder.Constant(3u))));
        return RefusedBecause(hull.program, "not affine");
    });

    passed &= run("a mask with a gap below the packed control point is refused", [] {
        Hull hull;
        auto& masked = hull.builder.BitwiseAnd(hull.packed(), hull.builder.Constant(0x1f0fu));
        hull.read(hull.inPatch(hull.builder.ShiftRightLogical(masked, hull.builder.Constant(3u))));
        return RefusedBecause(hull.program, "not affine");
    });

    passed &= run("a hull program's own bit of a whole-quad mask over a rectangle test lowers to the lane's own test when helper lanes are reset", [] {
        QuadWalk hull(Walk::Exact);
        IrValue* choice = hull.helperChoice;
        IrValue* predicate = hull.predicate;
        const Outcome outcome = Lower(hull.program);
        if (!outcome.lowered) std::fprintf(stderr, "refused: %s\n", outcome.reason.c_str());
        return outcome.lowered && !Contains(hull.program, IrOpcode::Ballot) && !Contains(hull.program, IrOpcode::WqmU64) && choice->Argument(0)->Resolve() == predicate->Resolve();
    });

    passed &= run("a whole-quad mask read refuses an LDS write in the block it gates", [] {
        return QuadRefusedBecause(Walk::StoreInBlock, {"(WriteSharedU32) in block $3 writes, exports or orders memory with a value that differs on a quad-helper lane", "(WriteSharedU32) in block $3 runs on a quad-helper lane in only one of the two programs"});
    });

    passed &= run("a whole-quad mask read refuses a DPP read in the block it gates", [] {
        return QuadRefusedBecause(Walk::NeighbourRead, {"(DppMoveU32) in block $3 hands another lane a value that differs on a quad-helper lane", "(DppMoveU32) in block $3 runs on a quad-helper lane in only one of the two programs"});
    });

    passed &= run("a whole-quad mask read refuses a value the block writes that is read after the loop without a reset", [] {
        return QuadRefusedBecause(Walk::UnresetValue, {"(StoreAddressU32) in block $6 writes, exports or orders memory with a value that differs on a quad-helper lane"});
    });

    passed &= run("a whole-quad mask read refuses a count of the mask", [] {
        return QuadRefusedBecause(Walk::CountMask, {"the mask is read other than as the lane's own bit, by %"});
    });

    passed &= run("a whole-quad mask read refuses a loop exit decided by the mask instead of the lane's test", [] {
        return QuadRefusedBecause(Walk::ExitOnMask, {"repeats the loop", "(Ballot) in block $2 hands another lane a value that differs on a quad-helper lane"});
    });

    passed &= run("a lane mask read other than through a whole-quad mask keeps the existing refusal", [] {
        Hull hull;
        auto& b = hull.builder;
        auto& lane = b.Emit(IrOpcode::LaneId, IrType::U32, {});
        auto& ballot = b.Emit(IrOpcode::Ballot, IrType::U32x4, {&b.Emit(IrOpcode::ULessThan32, IrType::Bool, {&lane, &b.Constant(3u)})});
        auto& count = b.Emit(IrOpcode::BitCount32, IrType::U32, {&b.Emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.Constant(0u)})});
        static_cast<void>(b.Emit(IrOpcode::ReferenceU32, IrType::Void, {&count}));
        const Outcome outcome = Lower(hull.program);
        if (outcome.lowered) return false;
        return outcome.reason.find("uses a lane mask other than its own lane's bit") != std::string::npos && outcome.reason.find("whole-quad") == std::string::npos;
    });

    passed &= run("the lowered rectangle walk matches a console wave model on random rectangle lists with quads spanning patches", [] {
        QuadWalk console(Walk::Exact);
        QuadWalk hull(Walk::Exact);
        const Outcome outcome = Lower(hull.program);
        if (!outcome.lowered) {
            std::fprintf(stderr, "refused: %s\n", outcome.reason.c_str());
            return false;
        }
        const ModelResult result = CompareWithConsole(console, hull.program, 300u);
        std::fprintf(stderr, "console model: %zu quad-helper block runs, %zu stores compared, %zu mismatches\n", result.helpers, result.compared, result.mismatches);
        return result.helpers > 0u && result.compared > 0u && result.mismatches == 0u;
    });

    passed &= run("the console wave model catches the unguarded lowering of a walk that leaves a helper's value live", [] {
        QuadWalk console(Walk::UnresetValue);
        QuadWalk hull(Walk::UnresetValue);
        hull.ownBit->ReplaceUsesWith(hull.predicate, true);
        const Outcome outcome = Lower(hull.program);
        if (!outcome.lowered) {
            std::fprintf(stderr, "refused: %s\n", outcome.reason.c_str());
            return false;
        }
        const ModelResult result = CompareWithConsole(console, hull.program, 300u);
        std::fprintf(stderr, "unguarded model: %zu quad-helper block runs, %zu stores compared, %zu mismatches\n", result.helpers, result.compared, result.mismatches);
        return result.helpers > 0u && result.mismatches > 0u;
    });

    passed &= run("a domain shader with primitive VGPR use on a non-export path lowers with constant zero", [] {
        IrProgram program;
        IrBuilder builder{program};
        program.Resources().stage = IrShaderStage::TessellationEvaluation;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        program.BlockOrder().push_back(&block);
        BlockInfo info;
        info.id = block.Id();
        program.Metadata().blockInfo.push_back(info);
        builder.SetInsertionPoint(block);
        auto& prim = builder.Emit(IrOpcode::TessellationBase, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(TessellationBaseKind::PassthroughPrimitive))});
        auto& ref = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&prim});
        static_cast<void>(ref);
        const Outcome outcome = Lower(program);
        if (!outcome.lowered) return false;
        return ref.Argument(0)->HasImmediate() && ref.Argument(0)->ImmediateU32() == 0u;
    });

    passed &= run("a hull shader with primitive VGPR use fails lowering", [] {
        IrProgram program;
        IrBuilder builder{program};
        program.Resources().stage = IrShaderStage::TessellationControl;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        program.BlockOrder().push_back(&block);
        BlockInfo info;
        info.id = block.Id();
        program.Metadata().blockInfo.push_back(info);
        builder.SetInsertionPoint(block);
        auto& prim = builder.Emit(IrOpcode::TessellationBase, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(TessellationBaseKind::PassthroughPrimitive))});
        auto& ref = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&prim});
        static_cast<void>(ref);
        const Outcome outcome = Lower(program);
        if (outcome.lowered) return false;
        return outcome.reason.find("the primitive VGPR is used other than as the passthrough primitive export") != std::string::npos;
    });

    return passed ? 0 : 1;
}
