#include "Optimization/SsaBuilder/SsaPass.hpp"
#include <cstddef>
#include <vector>

namespace ShaderRecompiler::Detail {

namespace {

enum class ReadStep { Start, SetValue, PushPhiArgument };

struct ReadState {
    IrBlock* block = nullptr;
    IrValue* result = nullptr;
    IrValue* phi = nullptr;
    std::size_t pred = 0;
    ReadStep step = ReadStep::Start;
};

}

void Pass::Write(Variable variable, IrBlock* block, IrValue* value) {
    std::visit([&](auto tag) { _definitions.Set(*block, tag, value); }, variable);
}

IrValue* Pass::Read(Variable variable, IrBlock* root) {
    std::vector<ReadState> stack{ReadState{}, ReadState{.block = root}};
    const auto preparePhi = [&]() {
        auto& state = stack.back();
        const auto& predecessors = state.block->Predecessors();
        if (state.pred == predecessors.size()) {
            IrValue* result = TryRemoveTrivialPhi(*state.phi, variable);
            IrBlock* block = state.block;
            stack.pop_back();
            stack.back().result = result;
            Write(variable, block, result);
            return;
        }
        state.step = ReadStep::PushPhiArgument;
        stack.push_back(ReadState{.block = predecessors[state.pred]});
    };
    do {
        auto& state = stack.back();
        IrBlock* block = state.block;
        switch (state.step) {
        case ReadStep::Start: {
            IrValue* def = std::visit([&](auto tag) { return _definitions.Get(*block, tag); }, variable);
            if (def != nullptr) {
                state.result = def;
            } else if (!block->IsSsaSealed()) {
                IrValue& phi = _program.CreateValue(IrOpcode::Phi, VariableType(variable));
                block->InsertInstructionBefore(nullptr, &phi);
                _incompletePhis[block][variable] = &phi;
                state.result = &phi;
            } else if (const auto& predecessors = block->Predecessors(); predecessors.size() == 1) {
                state.step = ReadStep::SetValue;
                stack.push_back(ReadState{.block = predecessors.front()});
                break;
            } else {
                IrValue& phi = _program.CreateValue(IrOpcode::Phi, VariableType(variable));
                block->InsertInstructionBefore(nullptr, &phi);
                Write(variable, block, &phi);
                state.phi = &phi;
                preparePhi();
                break;
            }
            [[fallthrough]];
        }
        case ReadStep::SetValue: {
            IrValue* result = state.result;
            Write(variable, block, result);
            stack.pop_back();
            stack.back().result = result;
            break;
        }
        case ReadStep::PushPhiArgument: {
            const auto& predecessors = block->Predecessors();
            state.phi->AddPhiOperand(predecessors[state.pred], state.result);
            state.pred++;
            preparePhi();
            break;
        }
        }
    } while (stack.size() > 1);
    return stack.back().result;
}

void Pass::Seal(IrBlock* block) {
    if (const auto found = _incompletePhis.find(block); found != _incompletePhis.end()) {
        for (auto& [variable, phi] : found->second) {
            const auto& irVal = AddPhiOperands(variable, *phi, block);
        }
    }
    block->SsaSeal();
}

IrValue* Pass::AddPhiOperands(Variable variable, IrValue& phi, IrBlock* block) {
    for (IrBlock* predecessor : block->Predecessors()) {
        phi.AddPhiOperand(predecessor, Read(variable, predecessor));
    }
    return TryRemoveTrivialPhi(phi, variable);
}

IrValue* Pass::TryRemoveTrivialPhi(IrValue& phi, Variable variable) {
    IrValue* same = nullptr;
    for (std::size_t index = 0; index < phi.ArgumentCount(); index++) {
        IrValue* operand = phi.Argument(index)->Resolve();
        if (operand == same || operand == &phi) {
            continue;
        }
        if (same != nullptr) {
            return &phi;
        }
        same = operand;
    }
    if (same == nullptr) {
        same = MakeInitialValue(variable);
    }
    std::vector<IrValue*> users = phi.Uses();
    phi.ReplaceUsesWith(same, true);
    for (IrValue* user : users) {
        if (user->IsPhi()) {
            const auto& irVal = TryRemoveTrivialPhi(*user, variable);
        }
    }
    return same;
}

IrValue* Pass::MakeInitialValue(Variable variable) {
    const IrType type = VariableType(variable);
    IrValue& value = _program.CreateValue(IrOpcode::Void, type);
    if (type == IrType::Bool) {
        value.SetImmediateBool(false);
    } else {
        value.SetImmediateU32(0u);
    }
    return &value;
}

}
