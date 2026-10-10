#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/MaskedSelectEliminator.hpp"
#include "Optimization/MaskedSelectVisited.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

using namespace ShaderRecompiler;

namespace {

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
        program.Metadata().blockInfo.push_back({.id = created.Id()});
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

bool expect(const char* name, bool condition) {
    if (!condition) std::fprintf(stderr, "%s\n", name);
    return condition;
}

bool run(const char* name, const std::function<bool()>& test) {
    try {
        return expect(name, test());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", name, error.what());
        return false;
    }
}

struct ScratchSwitch {
    std::optional<std::string> previous;

    static void set(const char* value) {
#ifdef _WIN32
        if (_putenv_s("APS5_NO_PERF_MASKED_SELECT_SCRATCH", value == nullptr ? "" : value) != 0) throw std::runtime_error("cannot set masked select scratch switch");
#else
        const auto result = value == nullptr ? unsetenv("APS5_NO_PERF_MASKED_SELECT_SCRATCH") : setenv("APS5_NO_PERF_MASKED_SELECT_SCRATCH", value, 1);
        if (result != 0) throw std::runtime_error("cannot set masked select scratch switch");
#endif
    }

    explicit ScratchSwitch(bool disabled) {
        if (const auto* value = std::getenv("APS5_NO_PERF_MASKED_SELECT_SCRATCH")) previous = value;
        set(disabled ? "1" : nullptr);
    }

    ~ScratchSwitch() {
#ifdef _WIN32
        _putenv_s("APS5_NO_PERF_MASKED_SELECT_SCRATCH", previous ? previous->c_str() : "");
#else
        if (previous) setenv("APS5_NO_PERF_MASKED_SELECT_SCRATCH", previous->c_str(), 1);
        else unsetenv("APS5_NO_PERF_MASKED_SELECT_SCRATCH");
#endif
    }
};

bool sameElimination(Builder& optimized, Builder& reference, std::uint32_t expected) {
    std::uint32_t originalCount = 0;
    {
        const ScratchSwitch original(true);
        originalCount = reference.eliminate();
    }
    std::uint32_t optimizedCount = 0;
    {
        const ScratchSwitch scratch(false);
        optimizedCount = optimized.eliminate();
    }
    return originalCount == expected && optimizedCount == originalCount && ProgramToString(optimized.program) == ProgramToString(reference.program);
}

bool differential(const std::function<void(Builder&)>& build, std::uint32_t expected) {
    Builder optimized;
    Builder reference;
    build(optimized);
    build(reference);
    if (ProgramToString(optimized.program) != ProgramToString(reference.program) || !sameElimination(optimized, reference, expected)) return false;
    const auto fixed = ProgramToString(optimized.program);
    return sameElimination(optimized, reference, 0u) && ProgramToString(optimized.program) == fixed;
}

template<typename Generation>
bool visitedIdentity() {
    IrProgram program;
    auto& block = program.CreateBlock();
    auto& first = program.CreateValue(IrOpcode::LaneId, IrType::U32);
    auto& second = program.CreateValue(IrOpcode::LaneId, IrType::U32);
    block.AppendInstruction(&first);
    block.AppendInstruction(&second);
    IrValue foreign(IrOpcode::LaneId, IrType::U32, first.Id());
    IrValue duplicateId(IrOpcode::LaneId, IrType::U32, first.Id());
    IrValue futureId(IrOpcode::LaneId, IrType::U32, static_cast<std::uint32_t>(program.Values().size()));
    IrValue sparse(IrOpcode::LaneId, IrType::U32, 1000000u);
    IrValue largest(IrOpcode::LaneId, IrType::U32, std::numeric_limits<std::uint32_t>::max());
    MaskedSelectVisited<Generation> visited(program);
    std::unordered_set<const IrValue*> reference;
    const auto insert = [&](const IrValue* value) {
        const bool expected = reference.insert(value).second;
        return visited.Insert(value) == expected && visited.Size() == reference.size();
    };
    visited.Begin();
    if (visited.Size() != 0u) return false;
    for (const auto* value : {&first, &foreign, &duplicateId, &first, &foreign, &sparse, &largest, &largest, &futureId}) {
        if (!insert(value)) return false;
    }
    auto& added = program.CreateValue(IrOpcode::LaneId, IrType::U32);
    block.AppendInstruction(&added);
    if (!insert(&added) || !insert(&added) || !insert(&futureId)) return false;
    block.RemoveInstruction(&first);
    first.Invalidate();
    if (!insert(&first)) return false;
    visited.Begin();
    reference.clear();
    if (visited.Size() != 0u || !insert(&first) || !insert(&added) || !insert(&foreign)) return false;
    MaskedSelectVisited<Generation> epochs(program);
    for (unsigned epoch = 0; epoch < 520u; ++epoch) {
        epochs.Begin();
        reference.clear();
        if (epochs.Size() != 0u) return false;
        const auto check = [&](const IrValue* value) {
            const bool expected = reference.insert(value).second;
            return epochs.Insert(value) == expected && epochs.Size() == reference.size();
        };
        if ((epoch == 0u || epoch >= 255u) && !check(&first)) return false;
        for (const auto* value : {&second, &foreign, &duplicateId, &sparse, &largest, &added, &foreign, &second}) {
            if (!check(value)) return false;
        }
    }
    return true;
}

}

int main() {
    bool passed = true;

    passed &= run("visited scratch preserves pointer identity and generation wrap", [] {
        return visitedIdentity<std::uint32_t>() && visitedIdentity<std::uint8_t>();
    });

    passed &= run("scratch preserves the seeded 4096 visit limit and duplicate user counting", [] {
        for (const auto count : {4095u, 4096u}) {
            for (const bool duplicateUse : {false, true}) {
                if (!differential([&](Builder& b) {
                    auto& exec = b.mask(16u);
                    auto& old = b.lane();
                    auto& written = b.select(exec, b.add(old, 1u), old);
                    auto& one = b.constant(1u);
                    auto* tail = &written;
                    for (unsigned index = 0; index < count; ++index) tail = &b.emit(IrOpcode::IAdd32, IrType::U32, {tail, duplicateUse ? tail : &one});
                    b.keep(b.select(exec, *tail, old));
                }, count == 4095u ? 1u : 0u)) return false;
            }
        }
        return true;
    });

    passed &= run("scratch preserves shared DAGs, distinct masks, pending cleanup and fixed-point mutation", [] {
        Builder optimized;
        Builder reference;
        for (auto* builder : {&optimized, &reference}) {
            auto& b = *builder;
            auto& firstExec = b.mask(16u);
            auto& secondExec = b.mask(32u);
            auto& old = b.lane();
            auto& first = b.select(firstExec, b.add(old, 1u), old);
            auto& shared = b.emit(IrOpcode::IAdd32, IrType::U32, {&first, &first});
            auto& left = b.add(shared, 2u);
            auto& right = b.add(shared, 3u);
            b.keep(b.select(firstExec, b.emit(IrOpcode::IAdd32, IrType::U32, {&left, &right}), old));
            auto& second = b.select(secondExec, b.add(old, 4u), old);
            b.keep(b.select(secondExec, b.add(second, 5u), old));
            b.emit(IrOpcode::ReadFirstLane, IrType::U32, {&second, &secondExec});
        }
        if (ProgramToString(optimized.program) != ProgramToString(reference.program) || !sameElimination(optimized, reference, 2u)) return false;
        if (!sameElimination(optimized, reference, 0u)) return false;
        for (auto* builder : {&optimized, &reference}) {
            auto& b = *builder;
            auto& exec = b.mask(8u);
            auto& old = b.lane();
            auto& written = b.select(exec, b.add(old, 6u), old);
            b.keep(b.select(exec, b.add(written, 7u), old));
        }
        return sameElimination(optimized, reference, 1u) && sameElimination(optimized, reference, 0u);
    });

    passed &= run("scratch preserves selects observed in a loop", [] {
        return differential([](Builder& b) {
            auto& exec = b.mask(16u);
            auto& old = b.lane();
            auto& outside = b.select(exec, b.add(old, 1u), old);
            auto& loop = b.newBlock();
            b.block->AddBranch(&loop);
            loop.AddBranch(&loop);
            b.block = &loop;
            auto& inside = b.select(exec, b.add(outside, 2u), old);
            b.keep(b.select(exec, b.add(inside, 3u), old));
        }, 0u);
    });

    passed &= run("a write read only under its own exec must lose its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(exec, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 1u && removed(written) && !removed(next);
    });

    passed &= run("a write read under a wider exec must keep its select", [] {
        Builder b;
        auto& wide = b.mask(32u);
        auto& exec = b.logicalAnd(wide, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(wide, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a write read under a narrower exec must lose its select", [] {
        Builder b;
        auto& exec = b.mask(32u);
        auto& narrow = b.logicalAnd(exec, b.mask(16u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(narrow, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 1u && removed(written);
    });

    passed &= run("a write whose old lanes flow into the next write must keep its select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& next = b.select(exec, b.add(old, 2u), written);
        b.keep(next);
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a chain of writes under one exec must collapse once the last one goes", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& first = b.select(exec, b.add(old, 1u), old);
        auto& second = b.select(exec, b.add(first, 2u), first);
        auto& sink = b.select(exec, b.add(second, 3u), old);
        b.keep(sink);
        return b.eliminate() == 2u && removed(first) && removed(second) && !removed(sink);
    });

    passed &= run("cross-lane reads must keep the select", [] {
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

    passed &= run("a branch condition derived from the write keeps the select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& taken = b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)});
        (void)b.emit(IrOpcode::Reference, IrType::Void, {&taken});
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a compare masked by the same exec must not keep the select", [] {
        Builder b;
        auto& exec = b.mask(16u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& compare = b.logicalAnd(exec, b.emit(IrOpcode::INotEqual32, IrType::Bool, {&written, &b.constant(0u)}));
        auto& ballot = b.emit(IrOpcode::Ballot, IrType::U32x4, {&compare});
        b.keep(b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&ballot, &b.constant(0u)}));
        return b.eliminate() == 1u && removed(written);
    });

    passed &= run("an exec rebuilt from a ballot of a narrower mask implies the write's exec", [] {
        Builder b;
        auto& exec = b.mask(48u);
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& later = b.ballotBit(b.logicalAnd(exec, b.mask(40u)));
        auto& next = b.select(later, b.add(written, 2u), old);
        b.keep(next);
        return b.eliminate() == 1u && removed(written);
    });

    passed &= run("an exec rebuilt from a ballot of a wider mask keeps the select", [] {
        Builder b;
        auto& wide = b.mask(48u);
        auto& exec = b.logicalAnd(wide, b.mask(40u));
        auto& old = b.lane();
        auto& written = b.select(exec, b.add(old, 1u), old);
        auto& later = b.ballotBit(wide);
        b.keep(b.select(later, b.add(written, 2u), old));
        return b.eliminate() == 0u && !removed(written);
    });

    passed &= run("a select under the full wave's exec must go whatever reads it", [] {
        Builder b;
        auto& full = b.threadBit(b.constant(0xffffffffu), b.constant(0xffffffffu));
        auto& old = b.lane();
        auto& written = b.select(full, b.add(old, 1u), old);
        b.keep(written);
        return b.eliminate() != 0u && removed(written);
    });

    passed &= run("a partial wave's exec is not the full wave", [] {
        Builder b;
        auto& partial = b.threadBit(b.constant(0xffffffffu), b.constant(0x0000ffffu));
        auto& old = b.lane();
        auto& written = b.select(partial, b.add(old, 1u), old);
        b.keep(written);
        return b.eliminate() == 0u && !removed(written);
    });

    const auto diamond = [](bool wideRead) {
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
    };

    passed &= run("a write merged by a phi and read under its own exec must lose its select", [&] {
        const auto [count, gone] = diamond(false);
        return count == 1u && gone;
    });

    passed &= run("a write merged by a phi and read under the wider exec must keep its select", [&] {
        const auto [count, gone] = diamond(true);
        return count == 0u && !gone;
    });

    passed &= run("a write in a loop must keep its select", [] {
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

    passed &= run("a write read inside a later loop must keep its select", [] {
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

    const auto sample = [](IrShaderStage stage, std::uint32_t sampleFlags) {
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
    };

    passed &= run("an implicit-LOD pixel sample reads its quad's coordinates and keeps the select", [&] {
        return !sample(IrShaderStage::Pixel, 0u);
    });

    passed &= run("an explicit-LOD sample reads only its lane and drops the select", [&] {
        return sample(IrShaderStage::Pixel, RdnaImageSampleFlagLevelZero) && sample(IrShaderStage::Mesh, 0u);
    });

    const auto guarded = [](IrOpcode opcode, bool sameExec, bool asActive) {
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
    };

    passed &= run("a store, load or export whose exec implies the write's exec drops the select", [&] {
        return guarded(IrOpcode::WriteSharedU32, true, false) && guarded(IrOpcode::LoadBufferU32, true, false) && guarded(IrOpcode::SetAttribute, true, false);
    });

    passed &= run("a store, load or export under a wider exec keeps the select", [&] {
        return !guarded(IrOpcode::WriteSharedU32, false, false) && !guarded(IrOpcode::LoadBufferU32, false, false) && !guarded(IrOpcode::SetAttribute, false, false);
    });

    passed &= run("a value used as a store's exec keeps the select", [&] {
        return !guarded(IrOpcode::WriteSharedU32, true, true);
    });

    passed &= run("an atomic keeps the select", [&] {
        return !guarded(IrOpcode::SharedAtomicIAdd32, true, false);
    });

    return passed ? 0 : 1;
}
