#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSAPASS_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSAPASS_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SsaBuilder/SsaDefTable.hpp"
#include "Optimization/SsaBuilder/SsaVariable.hpp"
#include <map>
#include <unordered_map>

namespace ShaderRecompiler::Detail {

class Pass {
public:
    explicit Pass(IrProgram& program) : _program(program) {}

    [[nodiscard]] IrValue* Read(Variable variable, IrBlock* root);
    void Write(Variable variable, IrBlock* block, IrValue* value);
    void Seal(IrBlock* block);

private:
    [[nodiscard]] IrValue* AddPhiOperands(Variable variable, IrValue& phi, IrBlock* block);
    [[nodiscard]] IrValue* TryRemoveTrivialPhi(IrValue& phi, Variable variable);
    [[nodiscard]] IrValue* MakeInitialValue(Variable variable);

    IrProgram& _program;
    DefTable _definitions;
    std::unordered_map<IrBlock*, std::map<Variable, IrValue*>> _incompletePhis;
};

}

#endif
