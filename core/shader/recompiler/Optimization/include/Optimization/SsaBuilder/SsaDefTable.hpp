#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSADEFTABLE_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSADEFTABLE_HPP

#include "IntermediateRepresentation/IrBlock.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include "Optimization/SsaBuilder/SsaVariable.hpp"
#include <cstdint>
#include <unordered_map>

namespace ShaderRecompiler::Detail {

class DefTable {
public:
    [[nodiscard]] IrValue* Get(IrBlock& block, ScalarReg reg) const;
    void Set(IrBlock& block, ScalarReg reg, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, ThreadBitScalarReg variable) const;
    void Set(IrBlock& block, ThreadBitScalarReg variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, ScalarMaskTag variable) const;
    void Set(IrBlock& block, ScalarMaskTag variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, VectorReg reg) const;
    void Set(IrBlock& block, VectorReg reg, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, GotoVariable variable) const;
    void Set(IrBlock& block, GotoVariable variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, SccTag variable) const;
    void Set(IrBlock& block, SccTag variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, ExecTag variable) const;
    void Set(IrBlock& block, ExecTag variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, ExecLoTag variable) const;
    void Set(IrBlock& block, ExecLoTag variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, ExecHiTag variable) const;
    void Set(IrBlock& block, ExecHiTag variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, VccTag variable) const;
    void Set(IrBlock& block, VccTag variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, VccLoTag variable) const;
    void Set(IrBlock& block, VccLoTag variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, VccHiTag variable) const;
    void Set(IrBlock& block, VccHiTag variable, IrValue* value);
    [[nodiscard]] IrValue* Get(IrBlock& block, M0Tag variable) const;
    void Set(IrBlock& block, M0Tag variable, IrValue* value);

private:
    std::unordered_map<IrBlock*, IrValue*> _scc;
    std::unordered_map<IrBlock*, IrValue*> _exec;
    std::unordered_map<IrBlock*, IrValue*> _execLo;
    std::unordered_map<IrBlock*, IrValue*> _execHi;
    std::unordered_map<IrBlock*, IrValue*> _vcc;
    std::unordered_map<IrBlock*, IrValue*> _vccLo;
    std::unordered_map<IrBlock*, IrValue*> _vccHi;
    std::unordered_map<IrBlock*, IrValue*> _m0;
    std::unordered_map<std::uint32_t, std::unordered_map<IrBlock*, IrValue*>> _gotoVariables;
};

}

#endif
