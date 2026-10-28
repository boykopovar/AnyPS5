#include "IntermediateRepresentation/IrProgram.hpp"
#include "IntermediateRepresentation/IrMetadata/StageIO.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/SingleLaneAnalysis.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Builder {
    IrProgram program;
    IrBlock* block = nullptr;
    ShaderWorkgroupInputInfo workgroup;

    explicit Builder(std::uint32_t width = 16u, std::uint32_t height = 16u) {
        block = &program.CreateBlock();
        program.BlockOrder().push_back(block);
        program.SetEntryBlock(*block);
        BlockInfo info;
        info.id = 0;
        program.Metadata().blockInfo.push_back(info);
        program.Resources().stage = IrShaderStage::Compute;
        program.SetWaveSize(64u);
        workgroup.threadsNum[0] = width;
        workgroup.threadsNum[1] = height;
        workgroup.threadsNum[2] = 1u;
    }

    IrValue& constant(std::uint32_t value) {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::U32);
        created.SetImmediateU32(value);
        return created;
    }

    IrValue& boolean(bool value) {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::Bool);
        created.SetImmediateBool(value);
        return created;
    }

    IrValue& emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        auto& created = program.CreateValue(opcode, type);
        for (auto* argument : arguments) created.AddArgument(argument);
        block->AppendInstruction(&created);
        return created;
    }

    IrValue& binary(IrOpcode opcode, IrValue& left, IrValue& right) {
        return emit(opcode, IrType::U32, {&left, &right});
    }

    IrValue& lane() {
        return emit(IrOpcode::LaneId, IrType::U32, {});
    }

    IrValue& localId(std::uint32_t axis) {
        return emit(IrOpcode::GetBuiltin, IrType::U32, {&constant(static_cast<std::uint32_t>(StageInputKind::LocalInvocationId)), &constant(axis)});
    }

    IrValue& userData() {
        return emit(IrOpcode::GetUserData, IrType::U32, {});
    }

    IrValue& laneDword() {
        auto& row = binary(IrOpcode::IMul32, constant(16u), emit(IrOpcode::BitFieldUExtract, IrType::U32, {&localId(1), &constant(0u), &constant(24u)}));
        return binary(IrOpcode::ShiftLeftLogical32, binary(IrOpcode::IAdd32, row, localId(0)), constant(2u));
    }

    IrValue& ballot(IrValue& predicate, std::uint32_t half) {
        auto& mask = emit(IrOpcode::Ballot, IrType::U32x4, {&predicate});
        return emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&mask, &constant(half)});
    }

    IrValue& threadBit(IrValue& low, IrValue& high) {
        auto& laneId = lane();
        auto& word = emit(IrOpcode::SelectU32, IrType::U32, {&emit(IrOpcode::ULessThan32, IrType::Bool, {&laneId, &constant(32u)}), &low, &high});
        auto& shift = binary(IrOpcode::BitwiseAnd32, laneId, constant(31u));
        auto& bit = binary(IrOpcode::BitwiseAnd32, binary(IrOpcode::ShiftRightLogical32, word, shift), constant(1u));
        return emit(IrOpcode::INotEqual32, IrType::Bool, {&bit, &constant(0u)});
    }

    IrValue& lds(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, std::uint32_t offset) {
        MemoryInfo memory;
        memory.kind = ResourceKind::Lds;
        memory.offset = offset;
        const auto index = static_cast<std::uint32_t>(program.Resources().memoryInfo.size());
        program.Resources().memoryInfo.push_back(memory);
        auto& created = emit(opcode, type, arguments);
        created.SetFlags(MemoryFlags{index, 0u});
        return created;
    }

    void keep(IrValue& value) {
        (void)emit(value.Type() == IrType::Bool ? IrOpcode::Reference : IrOpcode::ReferenceU32, IrType::Void, {&value});
    }

    void branch(BranchCondition condition, IrValue& predicate, IrValue* exec) {
        auto& info = program.Metadata().blockInfo.front();
        info.terminator.kind = TerminatorKind::ConditionalBranch;
        info.terminator.condition = condition;
        info.condition = &predicate;
        info.exec = exec;
    }

    IrBlock& addBlock() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        BlockInfo info;
        info.id = static_cast<std::uint32_t>(program.Metadata().blockInfo.size());
        program.Metadata().blockInfo.push_back(info);
        return created;
    }

    void at(IrBlock& target) {
        block = &target;
    }

    BlockInfo& infoOf(const IrBlock& target) {
        const auto& order = program.BlockOrder();
        return program.Metadata().blockInfo.at(static_cast<std::size_t>(std::find(order.begin(), order.end(), &target) - order.begin()));
    }

    void jump(IrBlock& from, IrBlock& to) {
        auto& info = infoOf(from);
        info.terminator.kind = TerminatorKind::Branch;
        info.terminator.trueBlock = infoOf(to).id;
        from.AddSuccessor(&to);
        to.AddPredecessor(&from);
    }

    void split(IrBlock& from, BranchCondition condition, IrValue& predicate, IrValue* exec, IrBlock& taken, IrBlock& otherwise) {
        auto& info = infoOf(from);
        info.terminator.kind = TerminatorKind::ConditionalBranch;
        info.terminator.condition = condition;
        info.terminator.trueBlock = infoOf(taken).id;
        info.terminator.falseBlock = infoOf(otherwise).id;
        info.condition = &predicate;
        info.exec = exec;
        for (IrBlock* target : {&taken, &otherwise}) {
            from.AddSuccessor(target);
            target->AddPredecessor(&from);
        }
    }

    IrValue& phi(IrType type, std::initializer_list<std::pair<IrBlock*, IrValue*>> incoming) {
        auto& created = program.CreateValue(IrOpcode::Phi, type);
        for (const auto& [from, value] : incoming) created.AddPhiOperand(from, value);
        block->AppendInstruction(&created);
        return created;
    }

    void fold() {
        (void)emit(IrOpcode::Return, IrType::Void, {});
        ConstantFolder{}.Fold(program);
    }

    SingleLaneVerdict analyze() const {
        return AnalyzeSingleLane(program, workgroup);
    }
};

bool eligible(const SingleLaneVerdict& verdict) {
    if (!verdict.eligible) std::fprintf(stderr, "  rejected: %s\n", verdict.reason);
    return verdict.eligible;
}

bool rejected(const SingleLaneVerdict& verdict, const char* reason) {
    if (verdict.eligible) {
        std::fprintf(stderr, "  accepted, expected \"%s\"\n", reason);
        return false;
    }
    if (std::strstr(verdict.reason, reason) != nullptr) return true;
    std::fprintf(stderr, "  rejected with \"%s\", expected \"%s\"\n", verdict.reason, reason);
    return false;
}

bool run(const char* name, const std::function<bool()>& test) {
    try {
        if (test()) return true;
        std::fprintf(stderr, "%s\n", name);
        return false;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", name, error.what());
        return false;
    }
}

}

int main() {
    bool passed = true;

    passed &= run("a select on a lane's bit of ~exec | exec folds to its true arm", [] {
        Builder b;
        auto& low = b.ballot(b.boolean(true), 0u);
        auto& high = b.ballot(b.boolean(true), 1u);
        auto& allLow = b.binary(IrOpcode::BitwiseOr32, b.emit(IrOpcode::BitwiseNot32, IrType::U32, {&low}), low);
        auto& allHigh = b.binary(IrOpcode::BitwiseOr32, b.emit(IrOpcode::BitwiseNot32, IrType::U32, {&high}), high);
        auto& active = b.localId(0);
        auto& selected = b.emit(IrOpcode::SelectU32, IrType::U32, {&b.threadBit(allLow, allHigh), &active, &b.constant(0u)});
        b.keep(selected);
        b.fold();
        return selected.Resolve() == &active;
    });

    passed &= run("a lane-private LDS array (tid * 4 + k * 1024) runs at one lane per invocation", [] {
        Builder b;
        auto& address = b.laneDword();
        auto& value = b.localId(0);
        (void)b.lds(IrOpcode::WriteSharedU32, IrType::Void, {&address, &value, &b.boolean(true)}, 0u);
        (void)b.lds(IrOpcode::WriteSharedU32, IrType::Void, {&address, &value, &b.boolean(true)}, 1024u);
        b.keep(b.lds(IrOpcode::LoadSharedU32, IrType::U32, {&address, &b.boolean(true)}, 1024u));
        return eligible(b.analyze());
    });

    passed &= run("a two-dimensional private LDS layout (4x + 1024y) runs at one lane", [] {
        Builder b;
        auto& address = b.binary(IrOpcode::IAdd32, b.binary(IrOpcode::ShiftLeftLogical32, b.localId(0), b.constant(2u)), b.binary(IrOpcode::ShiftLeftLogical32, b.localId(1), b.constant(10u)));
        (void)b.lds(IrOpcode::WriteSharedU32, IrType::Void, {&address, &b.localId(0), &b.boolean(true)}, 0u);
        return eligible(b.analyze());
    });

    passed &= run("lanes of one column sharing an LDS word keep two lanes", [] {
        Builder b;
        auto& address = b.binary(IrOpcode::ShiftLeftLogical32, b.localId(0), b.constant(2u));
        (void)b.lds(IrOpcode::WriteSharedU32, IrType::Void, {&address, &b.localId(1), &b.boolean(true)}, 0u);
        return rejected(b.analyze(), "share LDS");
    });

    passed &= run("an LDS read reaching the next lane's dword keeps two lanes", [] {
        Builder b;
        auto& address = b.laneDword();
        (void)b.lds(IrOpcode::WriteSharedU32, IrType::Void, {&address, &b.localId(0), &b.boolean(true)}, 0u);
        b.keep(b.lds(IrOpcode::LoadSharedU32x2, IrType::U32x2, {&address, &b.boolean(true)}, 0u));
        return rejected(b.analyze(), "share LDS");
    });

    passed &= run("LDS addressed by a loaded value keeps two lanes", [] {
        Builder b;
        auto& address = b.userData();
        (void)b.lds(IrOpcode::WriteSharedU32, IrType::Void, {&address, &b.localId(0), &b.boolean(true)}, 0u);
        return rejected(b.analyze(), "not affine");
    });

    passed &= run("LDS writes in a one-wave workgroup keep two lanes", [] {
        Builder b(8u, 8u);
        auto& address = b.binary(IrOpcode::ShiftLeftLogical32, b.binary(IrOpcode::IAdd32, b.binary(IrOpcode::ShiftLeftLogical32, b.localId(1), b.constant(3u)), b.localId(0)), b.constant(2u));
        (void)b.lds(IrOpcode::WriteSharedU32, IrType::Void, {&address, &b.localId(0), &b.boolean(true)}, 0u);
        return rejected(b.analyze(), "one-wave");
    });

    passed &= run("a lane's own ballot bit runs at one lane", [] {
        Builder b;
        auto& predicate = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(0), &b.constant(5u)});
        auto& ballot = b.emit(IrOpcode::Ballot, IrType::U32x4, {&predicate});
        auto& low = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.constant(0u)});
        auto& high = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.constant(1u)});
        auto& mask = b.emit(IrOpcode::BitwiseNot32, IrType::U32, {&low});
        auto& maskHigh = b.emit(IrOpcode::BitwiseNot32, IrType::U32, {&high});
        b.keep(b.emit(IrOpcode::SelectU32, IrType::U32, {&b.threadBit(mask, maskHigh), &b.constant(1u), &b.constant(0u)}));
        return eligible(b.analyze());
    });

    passed &= run("a counted ballot keeps two lanes", [] {
        Builder b;
        auto& low = b.ballot(b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(0), &b.constant(5u)}), 0u);
        b.keep(b.emit(IrOpcode::BitCount32, IrType::U32, {&low}));
        return rejected(b.analyze(), "lane mask read as a number");
    });

    passed &= run("a lane mask whose halves are different expressions keeps two lanes", [] {
        Builder b;
        auto& low = b.ballot(b.boolean(true), 0u);
        b.keep(b.emit(IrOpcode::SelectU32, IrType::U32, {&b.threadBit(low, b.constant(0u)), &b.constant(1u), &b.constant(0u)}));
        return rejected(b.analyze(), "halves differ");
    });

    passed &= run("the lane id read as data keeps two lanes", [] {
        Builder b;
        b.keep(b.binary(IrOpcode::IAdd32, b.lane(), b.constant(1u)));
        return rejected(b.analyze(), "lane id read as a number");
    });

    passed &= run("a workgroup of at most 32 lanes runs the same checks as a larger one", [] {
        Builder b(16u, 2u);
        b.keep(b.binary(IrOpcode::IAdd32, b.lane(), b.constant(1u)));
        return rejected(b.analyze(), "lane id read as a number");
    });

    passed &= run("a 32-thread group reading lane 63 in whole-wave mode keeps two lanes", [] {
        Builder b(32u, 1u);
        b.keep(b.emit(IrOpcode::ReadLane, IrType::U32, {&b.localId(0), &b.constant(63u)}));
        return rejected(b.analyze(), "lane read of a lane-varying value");
    });

    passed &= run("a barrier beside s_cbranch_execz in a 32-thread group keeps two lanes", [] {
        Builder b(32u, 1u);
        (void)b.emit(IrOpcode::Barrier, IrType::Void, {});
        auto& exec = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&b.localId(0), &b.constant(3u)});
        auto& skip = b.emit(IrOpcode::LogicalNot, IrType::Bool, {&exec});
        b.keep(skip);
        b.branch(BranchCondition::ExecZero, skip, nullptr);
        return rejected(b.analyze(), "barrier");
    });

    passed &= run("readfirstlane of user data runs at one lane, of a local id keeps two", [] {
        Builder uniform;
        uniform.keep(uniform.emit(IrOpcode::ReadFirstLane, IrType::U32, {&uniform.userData()}));
        Builder varying;
        varying.keep(varying.emit(IrOpcode::ReadFirstLane, IrType::U32, {&varying.localId(0)}));
        return eligible(uniform.analyze()) && rejected(varying.analyze(), "lane read of a lane-varying value");
    });

    passed &= run("a lane's vector window indexed by user data runs at one lane", [] {
        Builder b;
        (void)b.emit(IrOpcode::StoreVectorWindow, IrType::Void, {&b.userData(), &b.localId(0)});
        b.keep(b.emit(IrOpcode::LoadVectorWindow, IrType::U32, {&b.userData()}));
        return eligible(b.analyze());
    });

    passed &= run("readfirstlane of a vector window load keeps two lanes", [] {
        Builder b;
        (void)b.emit(IrOpcode::StoreVectorWindow, IrType::Void, {&b.constant(0u), &b.localId(0)});
        b.keep(b.emit(IrOpcode::ReadFirstLane, IrType::U32, {&b.emit(IrOpcode::LoadVectorWindow, IrType::U32, {&b.constant(0u)})}));
        return rejected(b.analyze(), "lane read of a lane-varying value");
    });

    passed &= run("a VCC branch on a uniform compare under the branch's exec runs at one lane", [] {
        Builder b;
        auto& exec = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&b.localId(0), &b.constant(3u)});
        auto& compare = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.userData(), &b.constant(5u)});
        auto& vcc = b.emit(IrOpcode::LogicalAnd, IrType::Bool, {&exec, &compare});
        auto& condition = b.emit(IrOpcode::LogicalNot, IrType::Bool, {&vcc});
        b.keep(condition);
        b.keep(exec);
        b.branch(BranchCondition::VccZero, condition, &exec);
        return eligible(b.analyze());
    });

    passed &= run("a VCC branch on a lane compare keeps two lanes", [] {
        Builder b;
        auto& exec = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&b.localId(0), &b.constant(3u)});
        auto& compare = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(1), &b.constant(5u)});
        auto& vcc = b.emit(IrOpcode::LogicalAnd, IrType::Bool, {&exec, &compare});
        b.keep(vcc);
        b.keep(exec);
        b.branch(BranchCondition::VccNonZero, vcc, &exec);
        return rejected(b.analyze(), "VCC branch on lane-varying bits");
    });

    passed &= run("a VCC branch whose bits came from a narrower exec keeps two lanes", [] {
        Builder b;
        auto& wide = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&b.localId(0), &b.constant(3u)});
        auto& narrow = b.emit(IrOpcode::LogicalAnd, IrType::Bool, {&wide, &b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(1), &b.constant(5u)})});
        auto& compare = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.userData(), &b.constant(5u)});
        auto& vcc = b.emit(IrOpcode::LogicalAnd, IrType::Bool, {&narrow, &compare});
        b.keep(vcc);
        b.keep(wide);
        b.branch(BranchCondition::VccNonZero, vcc, &wide);
        return rejected(b.analyze(), "VCC branch on lane-varying bits");
    });

    passed &= run("a VCC branch without its exec mask keeps two lanes", [] {
        Builder b;
        auto& compare = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.userData(), &b.constant(5u)});
        b.keep(compare);
        b.branch(BranchCondition::VccNonZero, compare, nullptr);
        return rejected(b.analyze(), "without its exec mask");
    });

    passed &= run("a barrier beside a VCC branch on exec-implied bits keeps two lanes", [] {
        Builder b;
        (void)b.emit(IrOpcode::Barrier, IrType::Void, {});
        auto& exec = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&b.localId(0), &b.constant(3u)});
        auto& compare = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.userData(), &b.constant(5u)});
        auto& vcc = b.emit(IrOpcode::LogicalAnd, IrType::Bool, {&exec, &compare});
        b.keep(vcc);
        b.keep(exec);
        b.branch(BranchCondition::VccNonZero, vcc, &exec);
        return rejected(b.analyze(), "barrier");
    });

    const auto skippable = [](Builder& b, IrBlock*& region, IrBlock*& merge) -> IrValue& {
        IrBlock& entry = *b.block;
        region = &b.addBlock();
        merge = &b.addBlock();
        auto& exec = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(0), &b.constant(32u)});
        auto& skip = b.emit(IrOpcode::LogicalNot, IrType::Bool, {&exec});
        b.split(entry, BranchCondition::ExecZero, skip, nullptr, *merge, *region);
        b.at(*region);
        return exec;
    };

    passed &= run("a scalar written under s_cbranch_execz and read per lane after the merge keeps two lanes", [&] {
        Builder b(64u, 1u);
        IrBlock* entry = b.block;
        IrBlock* region = nullptr;
        IrBlock* merge = nullptr;
        auto& old = b.userData();
        (void)skippable(b, region, merge);
        b.jump(*region, *merge);
        b.at(*merge);
        b.keep(b.phi(IrType::U32, {{entry, &old}, {region, &b.constant(1u)}}));
        return rejected(b.analyze(), "leaves a region one half can skip");
    });

    passed &= run("an exec-masked VGPR written under s_cbranch_execz runs at one lane", [&] {
        Builder b(64u, 1u);
        IrBlock* entry = b.block;
        IrBlock* region = nullptr;
        IrBlock* merge = nullptr;
        auto& old = b.localId(0);
        auto& exec = skippable(b, region, merge);
        auto& written = b.emit(IrOpcode::SelectU32, IrType::U32, {&exec, &b.binary(IrOpcode::IAdd32, old, b.constant(1u)), &old});
        b.jump(*region, *merge);
        b.at(*merge);
        b.keep(b.phi(IrType::U32, {{entry, &old}, {region, &written}}));
        return eligible(b.analyze());
    });

    passed &= run("a whole-wave VGPR write under s_cbranch_execz keeps two lanes", [&] {
        Builder b(64u, 1u);
        IrBlock* entry = b.block;
        IrBlock* region = nullptr;
        IrBlock* merge = nullptr;
        auto& old = b.localId(0);
        (void)skippable(b, region, merge);
        auto& written = b.binary(IrOpcode::IAdd32, old, b.constant(1u));
        b.jump(*region, *merge);
        b.at(*merge);
        b.keep(b.phi(IrType::U32, {{entry, &old}, {region, &written}}));
        return rejected(b.analyze(), "leaves a region one half can skip");
    });

    passed &= run("a VCC written under s_cbranch_execz and read after the merge keeps two lanes", [&] {
        Builder b(64u, 1u);
        IrBlock* entry = b.block;
        IrBlock* region = nullptr;
        IrBlock* merge = nullptr;
        auto& before = b.emit(IrOpcode::Ballot, IrType::U32x4, {&b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(1), &b.constant(3u)})});
        auto& beforeLow = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&before, &b.constant(0u)});
        auto& beforeHigh = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&before, &b.constant(1u)});
        auto& exec = skippable(b, region, merge);
        auto& compare = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(1), &b.constant(5u)});
        auto& after = b.emit(IrOpcode::Ballot, IrType::U32x4, {&b.emit(IrOpcode::LogicalAnd, IrType::Bool, {&compare, &exec})});
        auto& afterLow = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&after, &b.constant(0u)});
        auto& afterHigh = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&after, &b.constant(1u)});
        b.jump(*region, *merge);
        b.at(*merge);
        auto& low = b.phi(IrType::U32, {{entry, &beforeLow}, {region, &afterLow}});
        auto& high = b.phi(IrType::U32, {{entry, &beforeHigh}, {region, &afterHigh}});
        b.keep(b.emit(IrOpcode::SelectU32, IrType::U32, {&b.threadBit(low, high), &b.constant(1u), &b.constant(0u)}));
        return rejected(b.analyze(), "leaves a region one half can skip");
    });

    passed &= run("a vector window store under s_cbranch_execz must keep the skipped lanes' value", [&] {
        const auto build = [&](bool masked) {
            Builder b(64u, 1u);
            IrBlock* region = nullptr;
            IrBlock* merge = nullptr;
            auto& exec = skippable(b, region, merge);
            auto& index = b.constant(0u);
            auto& value = b.binary(IrOpcode::IAdd32, b.localId(0), b.constant(1u));
            auto& stored = masked ? b.emit(IrOpcode::SelectU32, IrType::U32, {&exec, &value, &b.emit(IrOpcode::LoadVectorWindow, IrType::U32, {&index})}) : value;
            (void)b.emit(IrOpcode::StoreVectorWindow, IrType::Void, {&index, &stored});
            b.jump(*region, *merge);
            b.at(*merge);
            b.keep(b.emit(IrOpcode::LoadVectorWindow, IrType::U32, {&index}));
            return b.analyze();
        };
        return eligible(build(true)) && rejected(build(false), "vector window store one half can skip");
    });

    const auto loop = [](bool counterUsedAfter) {
        Builder b(64u, 1u);
        IrBlock& entry = *b.block;
        IrBlock& header = b.addBlock();
        IrBlock& body = b.addBlock();
        IrBlock& exit = b.addBlock();
        auto& entryExec = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(0), &b.constant(40u)});
        auto& start = b.localId(1);
        b.jump(entry, header);
        b.at(header);
        auto& exec = b.phi(IrType::Bool, {{&entry, &entryExec}});
        auto& counter = b.phi(IrType::U32, {{&entry, &b.constant(0u)}});
        auto& carried = b.phi(IrType::U32, {{&entry, &start}});
        auto& done = b.emit(IrOpcode::LogicalNot, IrType::Bool, {&exec});
        b.split(header, BranchCondition::ExecZero, done, nullptr, exit, body);
        b.at(body);
        auto& next = b.binary(IrOpcode::IAdd32, counter, b.constant(1u));
        auto& nextExec = b.emit(IrOpcode::LogicalAnd, IrType::Bool, {&exec, &b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(0), &next})});
        auto& written = b.emit(IrOpcode::SelectU32, IrType::U32, {&nextExec, &b.binary(IrOpcode::IAdd32, carried, b.constant(2u)), &carried});
        exec.AddPhiOperand(&body, &nextExec);
        counter.AddPhiOperand(&body, &next);
        carried.AddPhiOperand(&body, &written);
        b.jump(body, header);
        b.at(exit);
        b.keep(counterUsedAfter ? counter : carried);
        return b.analyze();
    };

    passed &= run("an s_cbranch_execz loop with a scalar counter used after it keeps two lanes", [&] {
        return rejected(loop(true), "leaves a loop one half can leave early");
    });

    passed &= run("an s_cbranch_execz loop carrying an exec-masked VGPR out runs at one lane", [&] {
        return eligible(loop(false));
    });

    passed &= run("a scalar written under s_cbranch_execz keeps two lanes only when an instruction reads it", [&] {
        const auto build = [&](bool read) {
            Builder b(64u, 1u);
            IrBlock& entry = *b.block;
            IrBlock& header = b.addBlock();
            IrBlock& region = b.addBlock();
            IrBlock& merge = b.addBlock();
            IrBlock& exit = b.addBlock();
            auto& old = b.userData();
            auto& exec = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(0), &b.constant(32u)});
            b.jump(entry, header);
            b.at(header);
            auto& carried = b.phi(IrType::U32, {{&entry, &old}});
            b.split(header, BranchCondition::ExecZero, b.emit(IrOpcode::LogicalNot, IrType::Bool, {&exec}), nullptr, merge, region);
            b.jump(region, merge);
            b.at(merge);
            auto& merged = b.phi(IrType::U32, {{&header, &carried}, {&region, &b.constant(1u)}});
            carried.AddPhiOperand(&merge, &merged);
            b.split(merge, BranchCondition::SccNonZero, b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.userData(), &b.constant(5u)}), nullptr, header, exit);
            b.at(exit);
            if (read) b.keep(merged);
            return b.analyze();
        };
        return eligible(build(false)) && rejected(build(true), "leaves a region one half can skip");
    });

    passed &= run("an exec-masked VGPR carried through a loop inside an s_cbranch_execz region runs at one lane", [&] {
        Builder b(64u, 1u);
        IrBlock* entry = b.block;
        IrBlock* region = nullptr;
        IrBlock* merge = nullptr;
        auto& old = b.localId(0);
        auto& exec = skippable(b, region, merge);
        b.at(*entry);
        auto& kept = b.emit(IrOpcode::SelectU32, IrType::U32, {&exec, &b.binary(IrOpcode::IAdd32, old, b.constant(7u)), &old});
        IrBlock& header = b.addBlock();
        IrBlock& choose = b.addBlock();
        IrBlock& write = b.addBlock();
        IrBlock& join = b.addBlock();
        IrBlock& leave = b.addBlock();
        b.at(*region);
        auto& first = b.emit(IrOpcode::SelectU32, IrType::U32, {&exec, &b.binary(IrOpcode::IAdd32, old, b.constant(3u)), &old});
        b.jump(*region, header);
        b.at(header);
        auto& carried = b.phi(IrType::U32, {{region, &first}});
        b.split(header, BranchCondition::SccNonZero, b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.userData(), &b.constant(2u)}), nullptr, leave, choose);
        b.at(choose);
        b.split(choose, BranchCondition::SccNonZero, b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.userData(), &b.constant(3u)}), nullptr, write, join);
        b.at(write);
        auto& written = b.emit(IrOpcode::SelectU32, IrType::U32, {&exec, &b.binary(IrOpcode::IAdd32, carried, b.constant(1u)), &carried});
        b.jump(write, join);
        b.at(join);
        auto& joined = b.phi(IrType::U32, {{&choose, &carried}, {&write, &written}});
        carried.AddPhiOperand(&join, &joined);
        b.split(join, BranchCondition::SccNonZero, b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.userData(), &b.constant(4u)}), nullptr, header, leave);
        b.at(leave);
        auto& left = b.phi(IrType::U32, {{&header, &carried}, {&join, &joined}});
        b.jump(leave, *merge);
        b.at(*merge);
        b.keep(b.phi(IrType::U32, {{entry, &kept}, {&leave, &left}}));
        return eligible(b.analyze());
    });

    passed &= run("a loop whose exit test reads fewer lanes than its body runs keeps two lanes", [] {
        Builder b(64u, 1u);
        IrBlock& entry = *b.block;
        IrBlock& header = b.addBlock();
        IrBlock& body = b.addBlock();
        IrBlock& exit = b.addBlock();
        auto& entryAlive = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.localId(0), &b.constant(40u)});
        auto& entryState = b.binary(IrOpcode::BitwiseAnd32, b.localId(0), b.constant(1u));
        b.jump(entry, header);
        b.at(header);
        auto& alive = b.phi(IrType::Bool, {{&entry, &entryAlive}});
        auto& state = b.phi(IrType::U32, {{&entry, &entryState}});
        auto& exec = b.emit(IrOpcode::LogicalAnd, IrType::Bool, {&alive, &b.emit(IrOpcode::IEqual32, IrType::Bool, {&state, &b.constant(0u)})});
        b.split(header, BranchCondition::ExecZero, b.emit(IrOpcode::LogicalNot, IrType::Bool, {&exec}), nullptr, exit, body);
        b.at(body);
        auto& waiting = b.emit(IrOpcode::LogicalAnd, IrType::Bool, {&alive, &b.emit(IrOpcode::IEqual32, IrType::Bool, {&state, &b.constant(1u)})});
        auto& next = b.emit(IrOpcode::SelectU32, IrType::U32, {&waiting, &b.constant(0u), &b.emit(IrOpcode::SelectU32, IrType::U32, {&exec, &b.constant(2u), &state})});
        alive.AddPhiOperand(&body, &alive);
        state.AddPhiOperand(&body, &next);
        b.jump(body, header);
        b.at(exit);
        b.keep(state);
        return rejected(b.analyze(), "exec can widen");
    });

    passed &= run("the recompiler lays a qualifying wave64 program out at one lane unless APS5_NO_SINGLE_LANE_RULE is set", [] {
        const std::vector<std::uint32_t> code{0xbf810000u};
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x100000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.compute = ShaderComputeStageInfo{{64u, 2u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32u;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        const bool off = std::getenv("APS5_NO_SINGLE_LANE_RULE") != nullptr;
        return Recompile(request).hostSubgroupSize == (off ? 32u : 64u);
    });

    passed &= run("a DPP move keeps two lanes", [] {
        Builder b;
        b.keep(b.emit(IrOpcode::DppMoveU32, IrType::U32, {&b.localId(0)}));
        return rejected(b.analyze(), "operation across lanes");
    });

    passed &= run("a barrier in a program with exec branches keeps two lanes", [] {
        Builder b;
        (void)b.emit(IrOpcode::Barrier, IrType::Void, {});
        auto& exec = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&b.localId(0), &b.constant(3u)});
        auto& skip = b.emit(IrOpcode::LogicalNot, IrType::Bool, {&exec});
        b.keep(skip);
        b.branch(BranchCondition::ExecZero, skip, nullptr);
        return rejected(b.analyze(), "barrier");
    });

    if (!passed) return 1;
    std::printf("single lane tests passed\n");
    return 0;
}
