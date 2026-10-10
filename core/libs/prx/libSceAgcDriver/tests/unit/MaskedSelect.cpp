#include <Testing/Test.hpp>
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/MaskedSelectEliminator.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <utility>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;

struct Builder {
    IrProgram program;
    IrBlock* block = nullptr;

    Builder() {
        block = &newBlock();
        program.SetEntryBlock(*block);
    }

    IrBlock& newBlock() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        return created;
    }

    IrValue& constant(std::uint32_t value) {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::U32);
        created.SetImmediateU32(value);
        return created;
    }

    IrValue& emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, std::uint64_t flags = 0) {
        auto& created = program.CreateValue(opcode, type, flags);
        for (auto* argument : arguments) created.AddArgument(argument);
        block->AppendInstruction(&created);
        return created;
    }

    IrValue& lane() {
        return emit(IrOpcode::LaneId, IrType::U32, {});
    }

    IrValue& mask(std::uint32_t lanes) {
        return emit(IrOpcode::ULessThan32, IrType::Bool, {&lane(), &constant(lanes)});
    }

    IrValue& select(IrValue& condition, IrValue& active, IrValue& inactive) {
        return emit(IrOpcode::SelectU32, IrType::U32, {&condition, &active, &inactive});
    }

    IrValue& add(IrValue& value, std::uint32_t immediate) {
        return emit(IrOpcode::IAdd32, IrType::U32, {&value, &constant(immediate)});
    }

    IrValue& logicalAnd(IrValue& left, IrValue& right) {
        return emit(IrOpcode::LogicalAnd, IrType::Bool, {&left, &right});
    }

    IrValue& threadBit(IrValue& low, IrValue& high) {
        auto& laneId = lane();
        auto& word = select(emit(IrOpcode::ULessThan32, IrType::Bool, {&laneId, &constant(32u)}), low, high);
        auto& shift = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&laneId, &constant(31u)});
        auto& shifted = emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&word, &shift});
        auto& bit = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&shifted, &constant(1u)});
        return emit(IrOpcode::INotEqual32, IrType::Bool, {&bit, &constant(0u)});
    }

    IrValue& ballotBit(IrValue& predicate) {
        auto& ballot = emit(IrOpcode::Ballot, IrType::U32x4, {&predicate});
        auto& low = emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &constant(0u)});
        auto& high = emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &constant(1u)});
        return threadBit(low, high);
    }

    void keep(IrValue& value) {
        (void)emit(IrOpcode::ReferenceU32, IrType::Void, {&value});
    }

    std::uint32_t eliminate() {
        return MaskedSelectEliminator{}.Eliminate(program).removedSelects;
    }
};

bool removed(const IrValue& value) {
    return value.Parent() == nullptr;
}

void RequireEliminated(const char* name, const std::function<bool()>& test) {
    Testing::Require(test(), name);
}

const Case writeReadOnlyUnderItsOwnExec{"Eliminate_WriteReadOnlyUnderItsOwnExec_RemovesSelect", [] {
    RequireEliminated("a write read only under its own exec must lose its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(exec, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 1u && removed(written) && !removed(next);
    });
}};

const Case writeReadUnderWiderExec{"Eliminate_WriteReadUnderWiderExec_KeepsSelect", [] {
    RequireEliminated("a write read under a wider exec must keep its select", [] {
        Builder b;
        auto& wide = b.mask(32u);
        auto& exec = b.logicalAnd(wide, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(wide, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });
}};

const Case writeReadUnderNarrowerExec{"Eliminate_WriteReadUnderNarrowerExec_RemovesSelect", [] {
    RequireEliminated("a write read under a narrower exec must lose its select", [] {
        Builder b;
        auto& exec = b.mask(32u);
        auto& narrow = b.logicalAnd(exec, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(narrow, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 1u && removed(written);
    });
}};

const Case oldLanesFlowIntoNextWrite{"Eliminate_OldLanesFlowIntoNextWrite_KeepsSelect", [] {
    RequireEliminated("a write whose old lanes flow into the next write must keep its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(exec, b.add(old, 2u), written);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });
}};

const Case chainOfWritesUnderOneExec{"Eliminate_ChainOfWritesUnderOneExec_Collapses", [] {
    RequireEliminated("a chain of writes under one exec must collapse once the last one goes", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& first = b.select(exec, b.add(old, 1u), old);
        auto& second = b.select(exec, b.add(first, 2u), first);
        auto& sink = b.select(exec, b.add(second, 3u), old);
        b.keep(sink);
        return b.eliminate() == 2u && removed(first) && removed(second) && !removed(sink);
    });
}};

const Case crossLaneRead{"Eliminate_CrossLaneRead_KeepsSelect", [] {
    RequireEliminated("cross-lane reads must keep the select", [] {
        bool ok = true;
        for (const IrOpcode opcode : {IrOpcode::ReadFirstLane, IrOpcode::DppMoveU32, IrOpcode::BpermuteU32, IrOpcode::Permlane16U32, IrOpcode::SwizzleU32}) {
            Builder b;
            auto& exec = b.mask(16u);
            auto& old = b.lane();
            auto& written = b.select(exec, b.add(old, 1u), old);
            auto& read = b.emit(opcode, IrType::U32, {&written, &exec});
            b.keep(b.select(exec, read, old));
            ok &= b.eliminate() == 0u && !removed(written);
        }
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& ballot = b.emit(IrOpcode::Ballot, IrType::U32x4, {&b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)})});
        b.keep(b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.constant(0u)}));
        return ok && b.eliminate() == 0u && !removed(written);
    });
}};

const Case branchConditionFromWrite{"Eliminate_BranchConditionFromWrite_KeepsSelect", [] {
    RequireEliminated("a branch condition derived from the write keeps the select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& taken = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)});
        (void)b.emit(IrOpcode::Reference, IrType::Void, {&taken});
        return b.eliminate() == 0u && !removed(written);
    });
}};

const Case compareMaskedBySameExec{"Eliminate_CompareMaskedBySameExec_RemovesSelect", [] {
    RequireEliminated("a compare masked by the same exec must not keep the select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& compare = b.logicalAnd(exec, b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)}));
        auto& ballot = b.emit(IrOpcode::Ballot, IrType::U32x4, {&compare});
        b.keep(b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.constant(0u)}));
        return b.eliminate() == 1u && removed(written);
    });
}};

const Case execFromBallotOfNarrowerMask{"Eliminate_ExecFromBallotOfNarrowerMask_RemovesSelect", [] {
    RequireEliminated("an exec rebuilt from a ballot of a narrower mask implies the write's exec", [] {
        Builder b;
        auto& exec = b.mask(48u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& later = b.ballotBit(b.logicalAnd(exec, b.mask(40u)));
        auto& next = b.select(later, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 1u && removed(written);
    });
}};

const Case execFromBallotOfWiderMask{"Eliminate_ExecFromBallotOfWiderMask_KeepsSelect", [] {
    RequireEliminated("an exec rebuilt from a ballot of a wider mask keeps the select", [] {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(40u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& later = b.ballotBit(wide);
        b.keep(b.select(later, b.add(written, 2u), old));
        return b.eliminate() == 0u && !removed(written);
    });
}};

const Case selectUnderFullWaveExec{"Eliminate_SelectUnderFullWaveExec_RemovesSelect", [] {
    RequireEliminated("a select under the full wave's exec must go whatever reads it", [] {
        Builder b;
        auto& full = b.threadBit(b.constant(0xffffffffu), b.constant(0xffffffffu));
        auto& old = b.lane();
        auto& written = b.select(full, b.add(old, 1u), old);
        b.keep(written);
        return b.eliminate() != 0u && removed(written);
    });
}};

const Case selectUnderPartialWaveExec{"Eliminate_SelectUnderPartialWaveExec_KeepsSelect", [] {
    RequireEliminated("a partial wave's exec is not the full wave", [] {
        Builder b;
        auto& partial = b.threadBit(b.constant(0xffffffffu), b.constant(0x0000ffffu));
        auto& old = b.lane();
        auto& written = b.select(partial, b.add(old, 1u), old);
        b.keep(written);
        return b.eliminate() == 0u && !removed(written);
    });
}};

std::pair<std::uint32_t, bool> Diamond(bool wideRead) {
    Builder b;
    auto& wide = b.mask(48u);
    auto& exec = b.logicalAnd(wide, b.mask(16u));
    auto& old = b.lane();
    IrBlock& head = *b.block;
    IrBlock& body = b.newBlock();
    IrBlock& merge = b.newBlock();
    head.AddBranch(&body);
    head.AddBranch(&merge);
    body.AddBranch(&merge);
    b.block = &body;
    auto& written = b.select(exec, b.add(old, 1u), old);
    b.block = &merge;
    auto& phi = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
    merge.AppendInstruction(&phi);
    phi.AddPhiOperand(&head, &old);
    phi.AddPhiOperand(&body, &written);
    b.keep(b.select(wideRead ? wide : exec, b.add(phi, 2u), old));
    return std::pair{b.eliminate(), removed(written)};
}

const Case phiMergedWriteReadUnderOwnExec{"Eliminate_PhiMergedWriteReadUnderOwnExec_RemovesSelect", [] {
    RequireEliminated("a write merged by a phi and read under its own exec must lose its select", [] {
        const auto [count, gone] = Diamond(false);
        return count == 1u && gone;
    });
}};

const Case phiMergedWriteReadUnderWiderExec{"Eliminate_PhiMergedWriteReadUnderWiderExec_KeepsSelect", [] {
    RequireEliminated("a write merged by a phi and read under the wider exec must keep its select", [] {
        const auto [count, gone] = Diamond(true);
        return count == 0u && !gone;
    });
}};

const Case writeInLoop{"Eliminate_WriteInLoop_KeepsSelect", [] {
    RequireEliminated("a write in a loop must keep its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        IrBlock& head = *b.block;
        IrBlock& loop = b.newBlock();
        IrBlock& exit = b.newBlock();
        head.AddBranch(&loop);
        loop.AddBranch(&loop);
        loop.AddBranch(&exit);
        b.block = &loop;
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(exec, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });
}};

const Case writeReadInLaterLoop{"Eliminate_WriteReadInLaterLoop_KeepsSelect", [] {
    RequireEliminated("a write read inside a later loop must keep its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        IrBlock& head = *b.block;
        IrBlock& loop = b.newBlock();
        head.AddBranch(&loop);
        loop.AddBranch(&loop);
        b.block = &loop;
        b.keep(b.select(exec, b.add(written, 2u), old));
        return b.eliminate() == 0u && !removed(written);
    });
}};

bool Sample(IrShaderStage stage, std::uint32_t sampleFlags) {
    Builder b;
    b.program.Resources().stage = stage;
    MemoryInfo memory;
    memory.kind = ResourceKind::Image;
    memory.imageSampleFlags = sampleFlags;
    b.program.Resources().memoryInfo.push_back(memory);
    auto& exec = b.mask(16u);
    auto& old = b.lane();
    auto& written = b.select(exec, b.add(old, 1u), old);
    auto& address = b.emit(IrOpcode::MakeImageAddress, IrType::ImageAddress, {&written, &old});
    auto& image = b.emit(IrOpcode::GetImageResource, IrType::ImageResource, {});
    auto& sampler = b.emit(IrOpcode::GetSamplerResource, IrType::SamplerResource, {});
    MemoryFlags flags{0u, 0u};
    std::uint64_t bits = 0;
    std::memcpy(&bits, &flags, sizeof(flags));
    auto& sampled = b.emit(IrOpcode::ImageSampleRaw, IrType::U32x4, {&image, &sampler, &address}, bits);
    b.keep(b.select(exec, b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&sampled, &b.constant(0u)}), old));
    b.eliminate();
    return removed(written);
}

const Case implicitLodPixelSample{"Eliminate_ImplicitLodPixelSample_KeepsSelect", [] {
    RequireEliminated("an implicit-LOD pixel sample reads its quad's coordinates and keeps the select", [] {
        return !Sample(IrShaderStage::Pixel, 0u);
    });
}};

const Case explicitLodSample{"Eliminate_ExplicitLodSample_RemovesSelect", [] {
    RequireEliminated("an explicit-LOD sample reads only its lane and drops the select", [] {
        return Sample(IrShaderStage::Pixel, RdnaImageSampleFlagLevelZero) && Sample(IrShaderStage::Mesh, 0u);
    });
}};

bool Guarded(IrOpcode opcode, bool sameExec, bool asActive) {
    Builder b;
    auto& wide = b.mask(48u);
    auto& exec = b.logicalAnd(wide, b.mask(16u));
    auto& old = b.lane();
    auto& written = b.select(exec, b.add(old, 1u), old);
    IrValue& active = asActive ? b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)}) : sameExec ? exec : wide;
    switch (opcode) {
        case IrOpcode::WriteSharedU32:
            b.emit(opcode, IrType::Void, {&old, &written, &active});
            break;
        case IrOpcode::LoadBufferU32:
            b.keep(b.select(exec, b.emit(opcode, IrType::U32, {&b.emit(IrOpcode::GetBufferResource, IrType::BufferResource, {}), &written, &b.constant(0u), &b.constant(0u), &active}), old));
            break;
        default:
            b.emit(opcode, IrType::Void, {&b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&written, &old, &old, &old}), &active});
            break;
    }
    b.eliminate();
    return removed(written);
}

const Case guardedAccessUnderImpliedExec{"Eliminate_GuardedAccessUnderImpliedExec_RemovesSelect", [] {
    RequireEliminated("a store, load or export whose exec implies the write's exec drops the select", [] {
        return Guarded(IrOpcode::WriteSharedU32, true, false) && Guarded(IrOpcode::LoadBufferU32, true, false) && Guarded(IrOpcode::SetAttribute, true, false);
    });
}};

const Case guardedAccessUnderWiderExec{"Eliminate_GuardedAccessUnderWiderExec_KeepsSelect", [] {
    RequireEliminated("a store, load or export under a wider exec keeps the select", [] {
        return !Guarded(IrOpcode::WriteSharedU32, false, false) && !Guarded(IrOpcode::LoadBufferU32, false, false) && !Guarded(IrOpcode::SetAttribute, false, false);
    });
}};

const Case writeUsedAsStoreExec{"Eliminate_WriteUsedAsStoreExec_KeepsSelect", [] {
    RequireEliminated("a value used as a store's exec keeps the select", [] {
        return !Guarded(IrOpcode::WriteSharedU32, true, true);
    });
}};

const Case atomic{"Eliminate_Atomic_KeepsSelect", [] {
    RequireEliminated("an atomic keeps the select", [] {
        return !Guarded(IrOpcode::SharedAtomicIAdd32, true, false);
    });
}};

} // namespace
