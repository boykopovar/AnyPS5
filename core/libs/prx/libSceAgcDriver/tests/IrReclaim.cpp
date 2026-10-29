#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace ShaderRecompiler;

int main() {
    try {
        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        program.BlockOrder().push_back(&block);
        IrBuilder builder(program);
        builder.SetInsertionPoint(block);
        auto& value = builder.Constant(42u);
        IrSsaValues<NumScalarRegs> table;
        const auto& readTable = table;
        if (readTable[0] != nullptr || table.HasStorage()) throw std::runtime_error("an empty SSA table allocated storage during a read");
        for (IrValue* entry : readTable) if (entry != nullptr) throw std::runtime_error("an empty SSA table contained a value");
        if (table.HasStorage()) throw std::runtime_error("iterating an empty SSA table allocated storage");
        table[3] = &value;
        if (!table.HasStorage() || readTable[3] != &value || readTable[2] != nullptr) throw std::runtime_error("an SSA write lost zero initialization or its value");
        table.fill(nullptr);
        if (table.HasStorage() || readTable[3] != nullptr) throw std::runtime_error("clearing an SSA table retained construction storage");
        const auto id = value.Id();
        static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&value}));
        auto& identity = program.CreateValue(IrOpcode::Identity, IrType::U32);
        identity.AddArgument(&value);
        block.AppendInstruction(&identity);
        DeadCodeEliminator{}.RemoveIdentities(program);
        auto& first = program.CreateValue(IrOpcode::IAdd32, IrType::U32);
        auto& second = program.CreateValue(IrOpcode::IAdd32, IrType::U32);
        first.AddArgument(&second);
        second.AddArgument(&first);
        std::vector<IrValue*> pinned;
        for (std::uint32_t number = 100u; number < 112u; ++number) pinned.push_back(&builder.Constant(number));
        block.ssaScalarValues[0] = pinned[0];
        block.ssaVectorValues[0] = pinned[1];
        block.ssaThreadBitScalarValues[0] = pinned[2];
        block.ssaScalarMaskTags[0] = pinned[3];
        program.Metadata().dynamicReads.push_back(pinned[4]);
        program.Metadata().blockInfo.emplace_back();
        program.Metadata().blockInfo.back().condition = pinned[5];
        program.Metadata().blockInfo.back().indirectTarget = pinned[6];
        auto& resources = program.Resources();
        resources.descriptorSources.emplace_back();
        resources.descriptorSources.back().dwords[0] = pinned[7];
        resources.srtReads.push_back({pinned[8], 0u});
        resources.controlFlow.emplace_back();
        resources.controlFlow.back().condition = pinned[9];
        resources.uniformFill.values[0] = pinned[10];
        resources.valueStorage.Reserve(1);
        resources.valueStorage.Create(IrOpcode::Identity, IrType::U32, value.Id())->AddArgument(pinned[11]);
        const auto removed = program.ReclaimUnusedValues();
        if (removed != 3u) throw std::runtime_error("detached identity and cyclic garbage were not reclaimed");
        if (value.Id() != id || value.ImmediateU32() != 42u || value.Uses().size() != 1u) throw std::runtime_error("a live instruction lost its value or retained an unused backlink");
        for (std::size_t index = 0; index < pinned.size(); ++index) {
            if (pinned[index]->ImmediateU32() != 100u + index) throw std::runtime_error("a metadata or resource-owned value was reclaimed");
        }
        if (program.ReclaimUnusedValues() != 0u) throw std::runtime_error("reclaiming an unchanged graph was not idempotent");
        auto& unused = program.CreateValue(IrOpcode::IAdd32, IrType::U32);
        IrValue untracked(IrOpcode::Identity, IrType::U32, value.Id());
        untracked.AddArgument(&unused);
        bool rejected = false;
        try {
            static_cast<void>(program.ReclaimUnusedValues());
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected) throw std::runtime_error("an untracked external owner was accepted");
        untracked.ReplaceArgument(0, nullptr);
        program.Metadata().dynamicReads.clear();
        if (program.ReclaimUnusedValues() != 2u) throw std::runtime_error("reclamation did not recover after an external-owner rejection");
        if (builder.Constant(999u).Id() <= pinned.back()->Id()) throw std::runtime_error("reclamation reused a value ID");
        IrProgram moved = std::move(program);
        if (value.Id() != id || value.ImmediateU32() != 42u || moved.ReclaimUnusedValues() != 1u) throw std::runtime_error("moving a program invalidated its allocation owner");
        IrProgram assigned;
        static_cast<void>(assigned.CreateValue(IrOpcode::IAdd32, IrType::U32));
        assigned = std::move(moved);
        if (value.Id() != id || value.ImmediateU32() != 42u || assigned.ReclaimUnusedValues() != 0u) throw std::runtime_error("move assignment invalidated a live allocation owner");
        if (assigned.CreateValue(IrOpcode::IAdd32, IrType::U32).Id() <= pinned.back()->Id()) throw std::runtime_error("a transferred allocation owner reused a value ID");
        IrProgram operands;
        auto& entry = operands.CreateBlock();
        operands.SetEntryBlock(entry);
        operands.BlockOrder().push_back(&entry);
        IrBuilder wideBuilder(operands);
        wideBuilder.SetInsertionPoint(entry);
        std::vector<IrValue*> inputs;
        for (std::uint32_t number = 0; number < 7u; ++number) inputs.push_back(&wideBuilder.Constant(300u + number));
        auto& wide = wideBuilder.Emit(IrOpcode::ReferenceU32, IrType::Void, {inputs[0]});
        for (std::size_t index = 1; index < inputs.size(); ++index) wide.AddArgument(inputs[index]);
        if (wide.ArgumentCount() != inputs.size()) throw std::runtime_error("a value lost operands past its inline storage");
        for (std::size_t index = 0; index < inputs.size(); ++index) {
            if (wide.Argument(index) != inputs[index] || inputs[index]->UseCount() != 1u || inputs[index]->OperandUses().front().operand != index) throw std::runtime_error("an operand past the inline storage lost its value or its use");
        }
        wide.ReplaceArgument(5, inputs[0]);
        if (wide.Argument(5) != inputs[0] || inputs[5]->HasUses() || inputs[0]->UseCount() != 2u) throw std::runtime_error("replacing an operand in grown storage left a stale use");
        auto& detached = operands.CreateValue(IrOpcode::ReferenceU32, IrType::Void);
        detached.SetDetachedOperands({inputs[1], inputs[2], inputs[3], inputs[4], inputs[6]}, {});
        if (detached.ArgumentCount() != 5u || detached.Argument(0) != inputs[1] || detached.Argument(4) != inputs[6]) throw std::runtime_error("detached operands past the inline storage were not kept in order");
        if (operands.ReclaimUnusedValues() != 2u) throw std::runtime_error("an unused value with grown operand storage or its dropped operand was not reclaimed");
        for (std::size_t index = 0; index < inputs.size(); ++index) {
            if (wide.Argument(index) != (index == 5u ? inputs[0] : inputs[index])) throw std::runtime_error("reclamation changed a live value's grown operands");
        }
        std::puts("IR reclamation ownership tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
